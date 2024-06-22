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

mara_stack_frame_t*
mara_vm_alloc_stack_frame(mara_exec_ctx_t* ctx, mara_vm_closure_t* closure, mara_vm_state_t vm_state);

void
mara_vm_pop_stack_frame(mara_exec_ctx_t* ctx, mara_stack_frame_t* check_stackframe);

mara_error_t*
mara_vm_execute(mara_exec_ctx_t* ctx, mara_value_t* result);

#endif
