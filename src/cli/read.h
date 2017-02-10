#ifndef MEMCTL__CLI__READ_H_
#define MEMCTL__CLI__READ_H_

#include "memctl_types.h"

#include "cli/disassemble.h"

/*
 * memctl_read
 *
 * Description:
 * 	Read kernel memory and format the output to stdout.
 *
 * Parameters:
 * 		address			The kernel address to read.
 * 		size			The number of bytes to read.
 * 		physical		Read physical rather than virtual memory.
 * 		width			The formatting width.
 * 		access			The access width while reading.
 *
 * Returns:
 * 	true if the read was successful.
 */
bool memctl_read(kaddr_t address, size_t size, bool physical, size_t width, size_t access);

/*
 * memctl_dump
 *
 * Description:
 * 	Read kernel memory and write a dump output to stdout.
 *
 * Parameters:
 * 		address			The kernel address to read.
 * 		size			The number of bytes to read.
 * 		physical		Read physical rather than virtual memory.
 * 		width			The formatting width.
 * 		access			The access width while reading.
 *
 * Returns:
 * 	true if the read was successful.
 */
bool memctl_dump(kaddr_t address, size_t size, bool physical, size_t width, size_t access);

/*
 * memctl_dump_binary
 *
 * Description:
 * 	Dump raw kernel memory to stdout.
 *
 * Parameters:
 * 		address			The kernel address to read.
 * 		size			The number of bytes to read.
 * 		physical		Read physical rather than virtual memory.
 * 		access			The access width while reading.
 *
 * Returns:
 * 	true if the read was successful.
 */
bool memctl_dump_binary(kaddr_t address, size_t size, bool physical, size_t access);

#if MEMCTL_DISASSEMBLY

/*
 * memctl_disassemble
 *
 * Description:
 * 	Disassemble kernel memory to stdout.
 *
 * Parameters:
 * 		address			The kernel address to start disassembling at.
 * 		length			The number of bytes to read.
 * 		physical		Read physical rather than virtual memory.
 * 		access			The access width while reading.
 *
 * Returns:
 * 	true if the read was successful.
 */
bool memctl_disassemble(kaddr_t address, size_t length, bool physical, size_t access);

#endif // MEMCTL_DISASSEMBLY

/*
 * memctl_read_string
 *
 * Description:
 * 	Read the C-style string starting at the given address.
 *
 * Parameters:
 * 		address			The kernel address to read.
 * 		size			The maximum number of bytes to read.
 * 		physical		Read physical rather than virtual memory.
 * 		access			The access width while reading.
 *
 * Returns:
 * 	true if the read was successful.
 */
bool memctl_read_string(kaddr_t address, size_t size, bool physical, size_t access);

#endif
