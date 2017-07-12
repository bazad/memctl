#include "memctl/kernel_call.h"

#include "memctl/core.h"
#include "memctl/kernel.h"
#include "memctl/kernel_memory.h"
#include "memctl/memctl_signal.h"
#include "memctl/utility.h"
#include "memctl/vtable.h"

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
	bool success = false;
	bool retry   = true;
	for (size_t tries = 0; !success && retry; tries++) {
		if (tries == 5) {
			error_internal("could not open a connection to service with known "
			               "connection id: retry limit exceeded");
			return false;
		}
		success = open_service_with_known_connection_id_once(service,
				connection, connection_id, &retry);
	}
	return success;
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
				error_internal("found two registry entries with ID %llx\n", id);
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
	kext_result kr = vtable_for_class(user_client_name, kext_name, &hook.vtable,
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
 * 	Creates a connection to a user client, returning the connection and the address of the
 * 	user client instance in kernel memory.
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
 * 	for the IOExternalTrap object and initializes the offset field to 0.
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
		kaddr_t func, unsigned arg_count, const kword_t args[]) {
	if (arg_count > 7 || (arg_count > 0 && args[0] == 0) || result_size > sizeof(uint32_t)) {
		assert(func == 0);
		return false;
	}
	if (func == 0) {
		// If func is 0, then we can only support the given kernel call if the kernel_call
		// hook is installed.
		return hook.hooked;
	}
	assert(hook.hooked); // We better have already installed the hook.
	IOExternalTrap trap = { (args[0] == 0 ? 1 : args[0]), func, 0 };
	size_t size = sizeof(trap);
	kernel_io_result ior = kernel_write_unsafe(hook.trap, &size, &trap, 0, NULL);
	if (ior != KERNEL_IO_SUCCESS) {
		error_internal("could not write trap to kernel memory");
		return false;
	}
	uint32_t result32 = IOConnectTrap6(hook.connection, 0, args[1], args[2], args[3], args[4],
			args[5], args[6]);
	if (result_size > 0) {
		pack_uint(result, result32, result_size);
	}
	return true;
}

bool
kernel_call(void *result, unsigned result_size,
		kaddr_t func, unsigned arg_count, const kword_t args[]) {
	assert(result != NULL || func == 0 || result_size == 0);
	assert(ispow2(result_size) && result_size <= sizeof(uint64_t));
	assert(arg_count <= 8);
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
		kernel_deallocate(hook.hooked_vtable, hook.vtable_size);
		hook.hooked_vtable = 0;
	}
	if (hook.trap != 0) {
		kernel_deallocate(hook.trap, sizeof(IOExternalTrap));
		hook.trap = 0;
	}
}
