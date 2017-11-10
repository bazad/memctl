#include "memctl/kernel_call.h"

/*
 * Kernel Function Call Strategy
 * -----------------------------
 *
 *  At the point we are installing this kernel_call handler, we have access to the kernel_task, we
 *  know the kernel slide, and we have the basic kernel memory functions (kernel_write_unsafe,
 *  kernel_read_heap, etc.). We will use these primitives to hook an IOUserClient instance, which
 *  will allow us to call arbitrary kernel functions (albeit with some restrictions on the
 *  arguments and return value). This kernel function call mechanism is called kernel_call_7.
 *
 *  We replace the vtable pointer of an IOUserClient instance so that the iokit_user_client_trap
 *  Mach trap can be used to call arbitrary functions. This trap is reachable from user space via
 *  the IOConnectTrap6 function. This technique is described in Stefan Esser's slides titled "Tales
 *  from iOS 6 Exploitation and iOS 7 Security Challenges". While it's possible to devise an
 *  alternative calling mechanism (we could replace any method in the vtable), hooking IOUserClient
 *  traps is quite easy.
 *
 *  XNU's iokit_user_client_trap function determines which trap to invoke by calling the
 *  IOUserClient's getTargetAndTrapForIndex method. The default implementation of
 *  getTargetAndTrapForIndex calls getExternalTrapForIndex. The getExternalTrapForIndex method
 *  returns a pointer to an IOExternalTrap object, which is in turn returned by
 *  getTargetAndTrapForIndex. The iokit_user_client_trap function then invokes the trap by calling
 *  the trap->func function. The first argument in this call is trap->object (as extracted by
 *  getTargetAndTrapForIndex), and the remaining arguments are the values passed to IOConnectTrap6
 *  from user space. Thus, 7 arguments in total can be passed to the called function (hence the
 *  name kernel_call_7).
 *
 *  Unfortunately, using iokit_user_client_trap places some restrictions on the arguments and
 *  return value. Because the trap->object pointer is verified as non-NULL before the trap is
 *  invoked, the first argument to the called function cannot be 0. The more serious restriction is
 *  that the return value of iokit_user_client_trap is a kern_return_t, which on 64-bit platforms
 *  is a 32-bit type. This means for example that kernel_call_7 cannot be used to call functions
 *  that return a pointer, since the returned pointer value will be truncated. However, other
 *  kernel function call mechanisms can be established on top of kernel_call_7 that relax these
 *  restrictions.
 *
 *  To actually implement this hook, an AppleKeyStoreUserClient instance is allocated in the kernel
 *  by opening a connection to an AppleKeyStore service. I chose this class because most
 *  applications can access it on both macOS and iOS. The registry entry ID of the user client is
 *  determined by recording a list of the IDs of all children of AppleKeyStore and looking for a
 *  new child after the user client is created. (Despite searching far and wide, I could not find
 *  an official API to retrieve the registry entry ID associated with an io_connect_t object
 *  returned by IOServiceOpen.) Then, the kernel heap is scanned looking for an
 *  AppleKeyStoreUserClient instance with the same registry entry ID as the user client; if one is
 *  found, it is almost certainly the AppleKeyStoreUserClient instance we allocated earlier.
 *  (Ambiguity is eliminated by scanning the whole heap and failing if there is more than one
 *  match.) Having the address of this user client is crucial because we can now modify an
 *  IOUserClient instance to which we can send commands.
 *
 *  The next step is creating the fake vtable. The real AppleKeyStoreUserClient vtable is copied
 *  into user space and the IOUserClient::getExternalTrapForIndex method is replaced with
 *  IORegistryEntry::getRegistryEntryID. This means that when getTargetAndTrapForIndex calls
 *  getExternalTrapForIndex, the user client's registry entry ID will be returned instead. A block
 *  of kernel memory is allocated and the fake vtable is copied back into the kernel.
 *
 *  The last initialization step is to patch the AppleKeyStoreUserClient. More kernel memory is
 *  allocated to store the fake IOExternalTrap object, and the user client's registry entry ID
 *  field is overwritten with this address. Finally, the user client's vtable pointer is
 *  overwritten with the address of the fake vtable copied into the kernel earlier.
 *
 *  At this point, when iokit_user_client_trap calls getTargetAndTrapForIndex, the trap that is
 *  returned will be the address of the IOExternalTrap object we allocated. However, the fields of
 *  this object need to be initialized for each function call.
 *
 *  In order to actually call a kernel function, the IOExternalTrap object is overwritten so that
 *  the func field points to the function we want to call and the object field is the first
 *  argument to that function. Then, we can invoke IOConnectTrap6 with the remaining arguments to
 *  perform the actual function call.
 */

#include "memctl/class.h"
#include "memctl/core.h"
#include "memctl/kernel.h"
#include "memctl/kernel_memory.h"
#include "memctl/memctl_signal.h"
#include "memctl/utility.h"

#include "memctl_common.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

#if __arm64__
#include "aarch64/kernel_call_aarch64.h"
#elif __x86_64__
#include "memctl/x86_64/kernel_call_syscall_x86_64.h"
#endif


DEFINE_OFFSET(IORegistryEntry, reserved);
DEFINE_OFFSET(IORegistryEntry__ExpansionData, fRegistryEntryID);

static const char *service_name     = "AppleKeyStore";
static const char *user_client_name = "AppleKeyStoreUserClient";
#if KERNELCACHE
static const char *kext_name        = "com.apple.driver.AppleSEPKeyStore";
#else
static const char *kext_name        = "com.apple.driver.AppleKeyStore";
#endif
static const char *IOUserClient_getExternalTrapForIndex = "__ZN12IOUserClient23getExternalTrapForIndexEj";
static const char *IORegistryEntry_getRegistryEntryID   = "__ZN15IORegistryEntry18getRegistryEntryIDEv";

/*
 * hook
 *
 * Description:
 * 	The state needed to keep track of the IOKit trap hook.
 */
static struct iokit_trap_hook {
	// The connection to the user client.
	io_connect_t connection;
	// The address of the user client.
	kaddr_t user_client;
	// The original registry entry ID.
	uint64_t user_client_id;
	// The address of the registry entry ID.
	kaddr_t user_client_id_address;
	// The original vtable for the user client.
	kaddr_t vtable;
	// The size of the vtable.
	size_t vtable_size;
	// The hooked vtable, allocated with kernel_allocate.
	kaddr_t hooked_vtable;
	// The address of the IOExternalTrap in the kernel.
	kaddr_t trap;
	// Whether the vtable is currently hooked.
	bool hooked;
} hook;

/*
 * IOExternalTrap
 *
 * Description:
 * 	The kernel's IOExternalTrap struct. Defined in IOUserClient.h.
 */
typedef struct {
	kword_t object;
	kword_t func;
	kword_t offset;
} IOExternalTrap;

/*
 * get_child_ids
 *
 * Description:
 * 	Get the registry entry IDs of the children of the given IORegistryEntry.
 */
static bool
get_child_ids(io_registry_entry_t parent, size_t *count, uint64_t **child_ids) {
	// Create an iterator over the children of the parent service.
	io_iterator_t child_iterator = IO_OBJECT_NULL;
	kern_return_t kr = IORegistryEntryGetChildIterator(parent, "IOService", &child_iterator);
	if (kr != KERN_SUCCESS) {
		error_internal("could not create child iterator: %x", kr);
		return false;
	}
	bool success = false;
	uint64_t *ids = NULL;
	size_t capacity = 0;
	size_t found = 0;
	for (;;) {
		io_registry_entry_t child = IOIteratorNext(child_iterator);
		if (child == IO_OBJECT_NULL) {
			success = true;
			goto success;
		}
		// Allocate more space if needed.
		if (found == capacity) {
			capacity += 16;
			uint64_t *ids2 = realloc(ids, capacity * sizeof(*ids));
			if (ids2 == NULL) {
				error_out_of_memory();
				goto fail;
			}
			ids = ids2;
		}
		// Get the registry entry ID.
		uint64_t id;
		kr = IORegistryEntryGetRegistryEntryID(child, &id);
		IOObjectRelease(child);
		if (kr != KERN_SUCCESS) {
			error_internal("could not get ID of user client: %x", kr);
			goto fail;
		}
		// Store the ID in the array.
		ids[found] = id;
		found++;
	}
fail:
	free(ids);
	ids = NULL;
success:
	IOObjectRelease(child_iterator);
	*count = found;
	*child_ids = ids;
	return success;
}

/*
 * open_service_with_known_connection_id_once
 *
 * Description:
 * 	Open a connection to the service and find the registry entry ID of the client.
 */
static bool
open_service_with_known_connection_id_once(io_service_t service,
		io_connect_t *connection, uint64_t *connection_id, bool *retry) {
	bool successful = false;
	io_connect_t connect;
	// Get the current set of IDs.
	size_t    old_child_count;
	uint64_t *old_child_ids;
	bool success = get_child_ids(service, &old_child_count, &old_child_ids);
	if (!success) {
		goto fail_1;
	}
	// Open a connection to the service.
	kern_return_t kr = IOServiceOpen(service, mach_task_self(), 0, &connect);
	if (kr != KERN_SUCCESS) {
		error_internal("could not open service: %x", kr);
		goto fail_2;
	}
	// Get the new set of IDs.
	size_t    new_child_count;
	uint64_t *new_child_ids;
	success = get_child_ids(service, &new_child_count, &new_child_ids);
	if (!success) {
		goto fail_3;
	}
	// Try to find a single new child. This will be the connection ID.
	uint64_t connect_id = 0;
	for (size_t i = 0; i < new_child_count; i++) {
		uint64_t candidate = new_child_ids[i];
		for (size_t j = 0; j < old_child_count; j++) {
			if (candidate == old_child_ids[j]) {
				goto next;
			}
		}
		if (connect_id != 0) {
			*retry = true;
			goto fail_4;
		}
		connect_id = candidate;
next:;
	}
	assert(connect_id != 0);
	*connection    = connect;
	*connection_id = connect_id;
	successful = true;
fail_4:
	free(new_child_ids);
fail_3:
	if (!successful) {
		IOServiceClose(connect);
	}
fail_2:
	free(old_child_ids);
fail_1:
	return successful;
}

/*
 * open_service_with_known_connection_id
 *
 * Description:
 * 	Open a connection to the service and find the registry entry ID of the client.
 */
static bool
open_service_with_known_connection_id(io_service_t service,
		io_connect_t *connection, uint64_t *connection_id) {
	for (size_t tries = 0; tries < 5; tries++) {
		bool retry = false;
		bool success = open_service_with_known_connection_id_once(service,
				connection, connection_id, &retry);
		if (success || !retry) {
			return success;
		}
	}
	error_internal("could not open a connection to service with known "
	               "connection id: retry limit exceeded");
	return false;
}

/*
 * find_registry_entry_with_id
 *
 * Description:
 * 	Find an instance of any subclass of IORegistryEntry with the given vtable that has the
 * 	specified registry entry ID.
 *
 * Notes:
 * 	This is implemented by linearly scanning memory.
 */
static bool
find_registry_entry_with_id(kaddr_t vtable, uint64_t id, kaddr_t *object, kaddr_t *id_address) {
	// Scan heap memory to find all instances with the given vtable and figure out which one
	// has the given ID.
	*object = 0;
	kaddr_t kaddr = 0;
	error_stop();
	for (;;) {
		// Read a page of the kernel heap.
		uint8_t data[page_size];
		size_t readsize = sizeof(data);
		kaddr_t next;
		kernel_io_result ior = kernel_read_heap(kaddr, &readsize, data, 0, &next);
		if (interrupted) {
			error_start();
			error_interrupt();
			return false;
		}
		if (ior != KERNEL_IO_SUCCESS) {
			if (next == 0) {
				break;
			}
			kaddr = next;
			continue;
		}
		// Scan the page looking for IORegistryEntry instances with the expected vtable.
		size_t data_last = readsize - sizeof(kaddr_t);
		for (size_t data_offset = 0; data_offset <= data_last;
		     data_offset += sizeof(kaddr_t)) {
			if (*(kaddr_t *)(data + data_offset) != vtable) {
				continue;
			}
			// Get the value of the reserved field of the IORegistryEntry.
			kaddr_t reserved;
			size_t reserved_offset = data_offset + OFFSETOF(IORegistryEntry, reserved);
			if (reserved_offset <= data_last) {
				reserved = *(kaddr_t *)(data + reserved_offset);
			} else {
				ior = kernel_read_word(kernel_read_heap, kaddr + reserved_offset,
						&reserved, sizeof(reserved), 0);
				if (ior != KERNEL_IO_SUCCESS) {
					// We couldn't read the reserved field, so skip this one.
					continue;
				}
			}
			// Get the value of the fRegistryEntryID field of the ExpansionData.
			kaddr_t fRegistryEntryID_address =
				reserved + OFFSETOF(IORegistryEntry__ExpansionData, fRegistryEntryID);
			uint64_t fRegistryEntryID;
			ior = kernel_read_word(kernel_read_heap, fRegistryEntryID_address,
					&fRegistryEntryID, sizeof(fRegistryEntryID), 0);
			if (ior != KERNEL_IO_SUCCESS) {
				// The reserved field didn't point to valid heap memory, so skip
				// this one.
				continue;
			}
			if (fRegistryEntryID != id) {
				continue;
			}
			if (*object != 0) {
				error_start();
				error_internal("found two registry entries with ID %llx", id);
				*object = 0;
				return false;
			}
			*object = kaddr + data_offset;
			*id_address = fRegistryEntryID_address;
		}
		kaddr += readsize;
	}
	error_start();
	if (*object == 0) {
		error_internal("could not find address of registry entry");
		return false;
	}
	return true;
}

/*
 * get_user_client_vtable
 *
 * Description:
 * 	Get the vtable address and size of the user client class we will hook.
 *
 * Global state changes:
 * 	hook.vtable			The address of the user client's vtable.
 * 	hook.vtable_size		The size of the vtable.
 */
static bool
get_user_client_vtable() {
	kext_result kr = class_vtable(user_client_name, kext_name, &hook.vtable,
	                              &hook.vtable_size);
	if (kr != KEXT_SUCCESS) {
		error_internal("could not locate vtable for class %s", user_client_name);
		return false;
	}
	return true;
}

/*
 * create_user_client
 *
 * Description:
 * 	Creates the connection to the target user client and stores information about the user
 * 	client instance in the global state.
 *
 * Global state changes:
 * 	hook.connection			The connection to the user client.
 * 	hook.user_client		The address of the user client.
 * 	hook.user_client_id		The registry entry ID of the user client.
 * 	hook.user_client_id_address	The address of the registry entry ID field.
 * 	hook.vtable			The address of the user client's vtable.
 * 	hook.vtable_size		The size of the vtable.
 */
static bool
create_user_client() {
	// Get the user client's vtable and size.
	bool success = get_user_client_vtable();
	if (!success) {
		goto fail;
	}
	// Get the service.
	io_service_t service = IOServiceGetMatchingService(kIOMasterPortDefault,
			IOServiceMatching(service_name));
	if (service == IO_OBJECT_NULL) {
		error_internal("could not find service matching %s", service_name);
		goto fail;
	}
	// Open a connection to the service with a known registry entry ID.
	success = open_service_with_known_connection_id(service, &hook.connection,
			&hook.user_client_id);
	IOObjectRelease(service);
	if (!success) {
		goto fail;
	}
	// Find the address of the user client.
	success = find_registry_entry_with_id(hook.vtable, hook.user_client_id, &hook.user_client,
			&hook.user_client_id_address);
	if (!success) {
		goto fail;
	}
	return true;
fail:
	// We don't need to clean up the connection, it will be taken care of in kernel_call_init.
	return false;
}

/*
 * create_hooked_vtable
 *
 * Description:
 * 	Create a new vtable that replaces IOUserClient::getExternalTrapForIndex with
 * 	IOService::getRegistryEntryID.
 *
 * Global state changes:
 * 	hook.hooked_vtable		The newly allocated vtable.
 */
static bool
create_hooked_vtable() {
	// Get the address of the relevant kernel symbols.
	kaddr_t getExternalTrapForIndex;
	kaddr_t getRegistryEntryID;
	kext_result kxr = kernel_symbol(IOUserClient_getExternalTrapForIndex,
			&getExternalTrapForIndex, NULL);
	if (kxr != KEXT_SUCCESS) {
		error_internal("could not find %s::%s", "IOUserClient", "getExternalTrapForIndex");
		goto fail_1;
	}
	kxr = kernel_symbol(IORegistryEntry_getRegistryEntryID,
			&getRegistryEntryID, NULL);
	if (kxr != KEXT_SUCCESS) {
		error_internal("could not find %s::%s", "IORegistryEntry", "getRegistryEntryID");
		goto fail_1;
	}
	// Read the kernel vtable into a local buffer.
	size_t vtable_size = hook.vtable_size;
	kaddr_t *vtable_copy = malloc(vtable_size);
	if (vtable_copy == NULL) {
		error_out_of_memory();
		goto fail_1;
	}
	kernel_io_result ior = kernel_read_unsafe(hook.vtable, &vtable_size, vtable_copy, 0,
			NULL);
	if (ior != KERNEL_IO_SUCCESS) {
		error_internal("could not read %s vtable", user_client_name);
		goto fail_2;
	}
	// Replace IOUserClient::getExternalTrapForIndex with IORegistryEntry::getRegistryEntryID
	// in the local vtable buffer.
	bool found = false;
	for (size_t i = 0; i < vtable_size / sizeof(kaddr_t); i++) {
		if (vtable_copy[i] == getExternalTrapForIndex) {
			vtable_copy[i] = getRegistryEntryID;
			found = true;
		}
	}
	if (!found) {
		error_internal("%s vtable did not contain target method", user_client_name);
		goto fail_2;
	}
	// Allocate a replacement vtable in the kernel.
	bool success = kernel_allocate(&hook.hooked_vtable, vtable_size);
	if (!success) {
		goto fail_2;
	}
	// Write the hooked vtable into the kernel.
	ior = kernel_write_unsafe(hook.hooked_vtable, &vtable_size, vtable_copy, 0, NULL);
	free(vtable_copy);
	if (ior != KERNEL_IO_SUCCESS) {
		error_internal("could not write new vtable into kernel memory");
		goto fail_1;
	}
	return true;
fail_2:
	free(vtable_copy);
fail_1:
	// Leave deallocating the new vtable to kernel_call_init.
	return false;
}

/*
 * patch_user_client
 *
 * Description:
 * 	Patch the user client to use the hooked vtable. This function also allocates kernel memory
 * 	for the IOExternalTrap object.
 *
 * Global state changes:
 * 	hook.trap			The newly allocated IOExternalTrap.
 * 	hook.hooked			true
 */
bool
patch_user_client() {
	// Allocate the trap in the kernel.
	bool success = kernel_allocate(&hook.trap, sizeof(IOExternalTrap));
	if (!success) {
		goto fail;
	}
	// Set the registry entry ID of the user client to the trap.
	kernel_io_result ior = kernel_write_word(kernel_write_heap, hook.user_client_id_address,
			hook.trap, sizeof(hook.trap), 0);
	if (ior != KERNEL_IO_SUCCESS) {
		error_internal("could not set user client's registry entry ID");
		goto fail;
	}
	// Finally, replace the user client's vtable pointer.
	ior = kernel_write_word(kernel_write_unsafe, hook.user_client, hook.hooked_vtable,
			sizeof(hook.hooked_vtable), 0);
	if (ior != KERNEL_IO_SUCCESS) {
		error_internal("could not replace user client vtable");
		goto fail;
	}
	hook.hooked = true;
	return true;
fail:
	// Leave deallocating the trap to kernel_call_init.
	return false;
}

bool
kernel_call_7(void *result, unsigned result_size,
		kaddr_t func, unsigned arg_count, const struct kernel_call_argument args[]) {
	if (arg_count > 7 || (arg_count > 0 && args[0].value == 0)
			|| result_size > sizeof(uint32_t)) {
		assert(func == 0);
		return false;
	}
	if (func == 0) {
		// If func is 0, then we can only support the given kernel call if the kernel_call
		// hook is installed.
		return hook.hooked;
	}
	assert(hook.hooked); // We better have already installed the hook.
	// Get exactly 7 arguments. We initialize args7[0] to 1 in case there are no arguments.
	uint64_t args7[7] = { 1 };
	for (size_t i = 0; i < arg_count; i++) {
		args7[i] = args[i].value;
	}
	// Copy the IOExternalTrap into the kernel.
	IOExternalTrap trap = { args7[0], func, 0 };
	size_t size = sizeof(trap);
	kernel_io_result ior = kernel_write_unsafe(hook.trap, &size, &trap, 0, NULL);
	if (ior != KERNEL_IO_SUCCESS) {
		error_internal("could not write trap to kernel memory");
		return false;
	}
	// Trigger the function call and return the result.
	uint32_t result32 = IOConnectTrap6(hook.connection, 0, args7[1], args7[2], args7[3],
			args7[4], args7[5], args7[6]);
	if (result_size > 0) {
		pack_uint(result, result32, result_size);
	}
	return true;
}

bool
kernel_call(void *result, unsigned result_size,
		kaddr_t func, unsigned arg_count, const struct kernel_call_argument args[]) {
	assert(result != NULL || func == 0 || result_size == 0);
	assert(ispow2(result_size) && result_size <= sizeof(uint64_t));
	assert(arg_count <= 32);
#if __x86_64__
	if (kernel_call_syscall_x86_64(result, result_size, 0, arg_count, args)) {
		return kernel_call_syscall_x86_64(result, result_size, func, arg_count, args);
	} else
#endif
	if (kernel_call_7(result, result_size, 0, arg_count, args)) {
		return kernel_call_7(result, result_size, func, arg_count, args);
	}
#if __arm64__
	else if (kernel_call_aarch64(result, result_size, 0, arg_count, args)) {
		return kernel_call_aarch64(result, result_size, func, arg_count, args);
	}
#endif
	if (func != 0) {
		error_functionality_unavailable("kernel_call: no kernel_call implementation can "
		                                "perform the requested kernel function call");
	}
	return false;
}

bool
kernel_call_x(void *result, unsigned result_size,
		kaddr_t func, unsigned arg_count, const kword_t args[]) {
	assert(arg_count <= 8);
	struct kernel_call_argument xargs[8];
	for (size_t i = 0; i < arg_count; i++) {
		xargs[i].size  = sizeof(args[i]);
		xargs[i].value = args[i];
	}
	return kernel_call(result, result_size, func, arg_count, xargs);
}

static void
initialize_offsets() {
#define DEFAULT(base, object, value)			\
	if (OFFSET(base, object).valid == 0) {		\
		OFFSET(base, object).offset = value;	\
		OFFSET(base, object).valid  = 1;	\
	}
	DEFAULT(IORegistryEntry,                reserved,         2 * sizeof(kword_t));
	DEFAULT(IORegistryEntry__ExpansionData, fRegistryEntryID, 1 * sizeof(kword_t));
#undef DEFAULT
}

bool
kernel_call_init() {
	if (hook.hooked) {
		return true;
	}
	initialize_offsets();
	if (!create_user_client()) {
		error_internal("could not create a user client at a known address");
		goto fail;
	}
	if (!create_hooked_vtable()) {
		error_internal("could not create hooked vtable");
		goto fail;
	}
	if (!patch_user_client()) {
		error_internal("could not patch the user client");
		goto fail;
	}
#if __arm64__
	if (!kernel_call_init_aarch64()) {
		goto fail;
	}
#elif __x86_64__
	if (!kernel_call_init_syscall_x86_64()) {
		goto fail;
	}
#endif
	return true;
fail:
	kernel_call_deinit();
	return false;
}

void
kernel_call_deinit() {
#if __arm64__
	kernel_call_deinit_aarch64();
#elif __x86_64__
	kernel_call_deinit_syscall_x86_64();
#endif
	if (hook.hooked) {
		error_stop();
		kernel_write_word(kernel_write_unsafe, hook.user_client, hook.vtable,
				sizeof(hook.vtable), 0);
		error_start();
		hook.hooked = false;
	}
	if (hook.connection != IO_OBJECT_NULL) {
		IOServiceClose(hook.connection);
		hook.connection = IO_OBJECT_NULL;
	}
	if (hook.hooked_vtable != 0) {
		kernel_deallocate(hook.hooked_vtable, hook.vtable_size, false);
		hook.hooked_vtable = 0;
	}
	if (hook.trap != 0) {
		kernel_deallocate(hook.trap, sizeof(IOExternalTrap), false);
		hook.trap = 0;
	}
}
