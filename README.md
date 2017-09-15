## memctl

<!-- Brandon Azad -->

memctl is a kernel introspection tool that I developed to aid my security research into macOS and
iOS. It facilitates reverse engineering and vulnerability analysis in the kernel. It is not a
full-featured kernel debugger, but it can help illuminate what is happening in kernel memory.

The project is divided into two parts: a library called libmemctl and a command-line tool called
memctl. The libmemctl library provides functions to read and write kernel memory, call kernel
functions, find kernel symbols, and manipulate processes and tasks. The command-line tool wraps
this functionality into a debugger-like CLI so that it can be used on the device being analyzed.

In order to work memctl needs a library called a core to access to the kernel task port. I've
written the following cores:

* [memctl-tfp0-core]: A core for jailbroken iOS devices that uses `task_for_pid(0)` and
  `host_get_special_port(4)`.
* [memctl-kext-core]: A core for Macs that uses a custom kernel extension to access the kernel task
  port.
* [memctl-physmem-core]: A core for macOS 10.12.1 that leverages the physmem vulnerability to get
  the kernel task port.

[memctl-tfp0-core]: https://github.com/bazad/memctl-tfp0-core
[memctl-kext-core]: https://github.com/bazad/memctl-kext-core
[memctl-physmem-core]: https://github.com/bazad/memctl-physmem-core

An important design goal of libmemctl is to leave the kernel in a consistent state: it should clean
up any allocated kernel resources and restore all important system data it modifies before exiting.
Despite trying very hard to ensure that this goal is met, I cannot guarantee that using libmemctl
will not crash or corrupt your system. Use it at your own risk.

### Building

You will need a core in order to build memctl from source. Here is an example showing how to
compile memctl for iOS using the memctl-tfp0-core.

	$ git clone https://github.com/bazad/memctl
	$ cd memctl
	$ git clone https://github.com/bazad/memctl-tfp0-core
	$ cd memctl-tfp0-core
	$ make
	$ cd ..
	$ make ARCH=arm64 SDK=iphoneos CORE_DIR=memctl-tfp0-core

### Running

After successful compilation, the memctl binary is available at `bin/memctl`. Copy the binary to
the target device. Running memctl with no arguments will drop into a REPL. You can type `?` to see
a general list of commands, and type a specific command name followed by `?` to see specific help
for that command. Hit Ctrl-D or type `quit` to exit the REPL.

	$ memctl
	memctl> ?
	... help information ...
	memctl> r?
	... help information ...
	memctl> quit

### Usage examples

Here's a brief overview showing a couple of memctl's more useful commands.

We can find all instances of the class `AppleMobileFileIntegrity` with the command:

	memctl> fc AppleMobileFileIntegrity
	0xfffffff000def5a0

To find all occurrences of the pointer `0xfffffff000def5a0` on the heap:

	memctl> fh 0xfffffff000def5a0
	0xfffffff000d06ef0
	0xfffffff000d7dcf0
	0xfffffff000e0c700
	0xfffffff01743cbd8

Let's examine the first occurrence. We can find the zalloc allocation size for the allocation
containing the first address using:

	memctl> zs 0xfffffff000d06ef0
	128

Now let's read the memory surrounding address `0xfffffff000d06ef0`:

	memctl> r 0xfffffff000d06e00 256
	fffffff000d06e00:  0000000000000000 0000000000000000
	fffffff000d06e10:  0000000000000000 000000000000ffff
	fffffff000d06e20:  41656e4d00000000 0000000000000000
	fffffff000d06e30:  0000000000000000 0000000000008000
	fffffff000d06e40:  0000000000000002 0000000000000002
	fffffff000d06e50:  000000000000ffff 6934424100000000
	fffffff000d06e60:  0000000000000001 0000000000000001
	fffffff000d06e70:  414d50456d414964 deadbeefdeadbeef
	fffffff000d06e80:  fffffff0231980b0 0000000000000001
	fffffff000d06e90:  fffffff000de9500 fffffff000d7f280
	fffffff000d06ea0:  0000000000000000 0000000000000000
	fffffff000d06eb0:  0000000000000000 0000000000000000
	fffffff000d06ec0:  0000000000000000 fffffff000aee8f0
	fffffff000d06ed0:  0000000000000000 fffffff000b526e0
	fffffff000d06ee0:  000000000000001e 0000000000000000
	fffffff000d06ef0:  fffffff000def5a0 deadbeefdeadbeef

The presence of `deadbeefdeadbeef` suggests that address `0xfffffff000d06e70` is at the end of one
allocation and address `0xfffffff000d06e80` is the start of the next one. However, before we move
on, the data at address `0xfffffff000d06e70` looks like ASCII. We can dump memory contents with
ASCII annotations by adding the `d` flag, or we can print the string directly using the `rs`
command:

	memctl> rd fffffff000d06e70 8
	fffffff000d06e70:  6449416d45504d41                   |dIAmEPMA        |
	memctl> rs fffffff000d06e70 8
	dIAmEPMA

We can determine what type of object is at address `0xfffffff000d06e80` using the `lc` command:

	memctl> lc fffffff000d06e80
	AppleSmartIOCommand

Once we know the class name, we can inspect its metaclass instance to determine the class size. The
`a` command will print the address and size of a symbol by name and, optionally, bundle identifier:

	memctl> a :__ZN19AppleSmartIOCommand10gMetaClassE
	0xfffffff0238adff8  (40)
	memctl> r 0xfffffff0238adff8 40
	fffffff0238adff8:  fffffff023198138 fffffff000d58b70
	fffffff0238ae008:  fffffff0237b6930 fffffff000d5e640
	fffffff0238ae018:  0000025000000070
	memctl> r4 fffffff0238ae018
	fffffff0238ae018:  00000070

In this case the class size is `0x70`, which means the `AppleMobileFileIntegrity` pointer at
address `0xfffffff000d06ef0` is likely leftover heap garbage.

### License

memctl is released under the MIT license.

memctl also relies on some third-party source code, most of which is released by Apple under the
terms of the Apple Public Source License. All third party source code is placed in the external
directory and is not considered to be part of memctl. Third party code remains under the original
licensing terms.
