#ifndef MEMCTL_CLI__DISASSEMBLE_H_
#define MEMCTL_CLI__DISASSEMBLE_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/*
 * MEMCTL_DISASSEMBLY
 *
 * Description:
 * 	Whether disassembly is supported on this platform.
 */
#ifndef MEMCTL_DISASSEMBLY
# if __arm64__
#  define MEMCTL_DISASSEMBLY 1
# endif
#endif

#if MEMCTL_DISASSEMBLY

/*
 * disassemble
 *
 * Description:
 * 	Disassemble the given sequence of bytes, printing the disassembly to stdout.
 *
 * Parameters:
 * 		ins			The sequence of instructions.
 * 	inout	size			The size of the buffer. On return, this is the number of
 * 					bytes left unprocessed.
 * 	inout	count			The maximum number of instructions to disassemble. On
 * 					return, this is decreased by the number of instructions
 * 					processed.
 * 		pc			The address of the instructions in memory.
 */
void disassemble(const void *ins, size_t *size, size_t *count, uintptr_t pc);

#endif // MEMCTL_DISASSEMBLY

#endif
