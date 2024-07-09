#ifndef MARA_VM_H
#define MARA_VM_H

#include "internal.h"

MARA_PRIVATE void
mara_decode_instruction(
	mara_instruction_t instruction,
	mara_opcode_t* opcode,
	mara_operand_t* operands
) {
	*opcode = (uint8_t)((instruction >> 24) & 0xff);
	*operands = instruction & 0x00ffffff;
}

MARA_PRIVATE mara_instruction_t
mara_encode_instruction(
	mara_opcode_t opcode,
	mara_operand_t operands
) {
	return (((uint32_t)opcode & 0xff) << 24) | (operands & 0x00ffffff);
}

MARA_PRIVATE const char*
mara_opcode_to_str(mara_opcode_t opcode) {
#define MARA_OPCODE_TO_STR(OP) case MARA_OP_##OP: return #OP;

	switch (opcode) {
		MARA_OPCODE(MARA_OPCODE_TO_STR);
		default:
			return "INVALID";
	}
}

#endif
