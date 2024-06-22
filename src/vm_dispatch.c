#include "internal.h"
#include "vm.h"

#if defined(__GNUC__) || defined(__clang__)
#   define MARA_MAKE_DISPATCH_TABLE(X) &&MARA_OP_##X,
#   define MARA_DISPATCH_NEXT() \
        do { \
            mara_instruction_t instruction = *ip; \
            ++ip; \
            mara_decode_instruction(instruction, &opcode, &operands); \
        } while (0); \
        goto *dispatch_table[opcode];
#   define MARA_BEGIN_DISPATCH(OPCODE) \
        static void* dispatch_table[] = { \
            MARA_OPCODE(MARA_MAKE_DISPATCH_TABLE) \
        }; \
        MARA_DISPATCH_NEXT()
#   define MARA_BEGIN_OP(NAME) MARA_OP_##NAME:
#   define MARA_END_OP() MARA_DISPATCH_NEXT()
#   define MARA_END_DISPATCH()
#else
#   define MARA_BEGIN_DISPATCH(OPCODE) \
        while (true) { \
            mara_assert(sp <= fp->stack + closure->fn->stack_size, "Stack overflow"); \
            mara_assert((fp->stack + closure->fn->num_locals) <= sp, "Stack underflow"); \
            mara_instruction_t instruction = *ip; \
            ++ip; \
            mara_decode_instruction(instruction, &opcode, &operands); \
            switch (opcode) {
#   define MARA_BEGIN_OP(NAME) case MARA_OP_##NAME:
#   define MARA_END_OP() continue;
#   define MARA_END_DISPATCH() }}
#endif

mara_error_t*
mara_vm_execute(mara_exec_ctx_t* ctx, mara_value_t* result) {
#define MARA_VM_SAVE_STATE(STATE) \
	do { \
		(STATE)->args = args; \
		(STATE)->sp = sp; \
		(STATE)->ip = ip; \
		(STATE)->fp = fp; \
	} while (0)

#define MARA_VM_LOAD_STATE(STATE) \
	do { \
		args = (STATE)->args; \
		sp = (STATE)->sp; \
		ip = (STATE)->ip; \
		fp = (STATE)->fp; \
	} while (0)

#define MARA_VM_DERIVE_STATE() \
	do { \
		stack_top = *sp; \
		locals = fp->stack; \
		closure = fp->closure; \
		captures = closure->captures; \
		constants = closure->fn->constants; \
		functions = closure->fn->functions; \
		closure_header = mara_container_of(closure, mara_obj_t, body); \
	} while (0)

	mara_instruction_t* ip;
	mara_stack_frame_t* fp;
	mara_value_t* sp;
	mara_value_t* args;

	mara_vm_closure_t* closure;
	mara_value_t* captures;
	mara_value_t* locals;
	mara_value_t* constants;
	mara_vm_function_t** functions;
	mara_obj_t* closure_header;
	mara_value_t stack_top;

	mara_vm_state_t* vm = &ctx->vm_state;
	MARA_VM_LOAD_STATE(vm);
	MARA_VM_DERIVE_STATE();

    mara_opcode_t opcode;
    mara_operand_t operands;

    MARA_WARNING_PUSH()
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wgnu-label-as-value"
#endif
		// TODO: computed goto and switched goto
    MARA_BEGIN_DISPATCH(opcode)
        MARA_BEGIN_OP(NOP)
        MARA_END_OP()
        MARA_BEGIN_OP(NIL)
            *(++sp) = stack_top = mara_nil();
        MARA_END_OP()
        MARA_BEGIN_OP(TRUE)
            *(++sp) = stack_top = mara_value_from_bool(true);
        MARA_END_OP()
        MARA_BEGIN_OP(FALSE)
            *(++sp) = stack_top = mara_value_from_bool(false);
        MARA_END_OP()
        MARA_BEGIN_OP(SMALL_INT)
            *(++sp) = stack_top = mara_value_from_int((int16_t)(operands & 0xffff));
        MARA_END_OP()
        MARA_BEGIN_OP(CONSTANT)
            // Constants come from permanent zone so copying is needed
            *(++sp) = stack_top = mara_copy(ctx, ctx->current_zone, constants[operands]);
        MARA_END_OP()
        MARA_BEGIN_OP(POP)
            sp -= operands;
            stack_top = *sp;
        MARA_END_OP()
        MARA_BEGIN_OP(SET_LOCAL)
            locals[operands] = stack_top;
        MARA_END_OP()
        MARA_BEGIN_OP(GET_LOCAL)
            *(++sp) = stack_top = locals[operands];
        MARA_END_OP()
        MARA_BEGIN_OP(SET_ARG)
            // TODO: Rethink this opcode
            // The arguments may come from a list in mara_apply
            // Some zone crossing might happen.
            args[operands] = stack_top;
        MARA_END_OP()
        MARA_BEGIN_OP(GET_ARG)
            *(++sp) = stack_top = args[operands];
        MARA_END_OP()
        MARA_BEGIN_OP(SET_CAPTURE)
            {
                mara_value_t value_copy = mara_copy(ctx, closure_header->zone, stack_top);
                captures[operands] = value_copy;
                mara_obj_add_arena_mask(closure_header, value_copy);
            }
        MARA_END_OP()
        MARA_BEGIN_OP(GET_CAPTURE)
            *(++sp) = stack_top = captures[operands];
        MARA_END_OP()
        MARA_BEGIN_OP(CALL)
            {
                mara_obj_t* fn = mara_value_to_obj(stack_top);
                if (
                    MARA_EXPECT(
                        fn != NULL
                        && (
                            fn->type == MARA_OBJ_TYPE_VM_CLOSURE
                            || fn->type == MARA_OBJ_TYPE_NATIVE_CLOSURE
                        )
                    )
                ) {
                    mara_vm_closure_t* next_closure;
                    if (fn->type == MARA_OBJ_TYPE_VM_CLOSURE) {
                        next_closure = (mara_vm_closure_t*)fn->body;
                    } else {
                        next_closure = NULL;
                    }

                    mara_zone_t* return_zone = ctx->current_zone;

                    // New stack frame
                    sp -= operands;
                    mara_vm_state_t frame_state;
                    MARA_VM_SAVE_STATE(&frame_state);
                    mara_stack_frame_t* stackframe = mara_vm_alloc_stack_frame(
                        ctx, next_closure, frame_state
                    );

                    mara_zone_enter_new(
                        ctx, (mara_zone_options_t){
                            .argc = operands,
                            .argv = sp,
                        }
                    );

                    if(next_closure != NULL) {
                        if (MARA_EXPECT(next_closure->fn->num_args <= (mara_index_t)operands)) {
                            args = sp;
                            fp = stackframe;
                            sp = stackframe->stack + next_closure->fn->num_locals;
                            ip = next_closure->fn->instructions;
                            MARA_VM_DERIVE_STATE();
                        } else {
                            MARA_VM_SAVE_STATE(vm);
                            return mara_errorf(
                                ctx, mara_str_from_literal("core/wrong-arity"),
                                "Function expects %d arguments, got %d",
                                mara_nil(),
                                next_closure->fn->num_args, operands
                            );
                        }
                    } else {
                        args = sp;
                        fp = stackframe;
                        sp = NULL;
                        ip = NULL;
                        MARA_VM_SAVE_STATE(vm);

                        mara_native_closure_t* native_closure = (mara_native_closure_t*)fn->body;
                        mara_value_t result = mara_nil();
                        mara_check_error(
                            native_closure->fn(
                                ctx,
                                operands, args,
                                native_closure->userdata,
                                &result
                            )
                        );
                        mara_value_t result_copy = mara_copy(ctx, return_zone, result);

                        mara_vm_pop_stack_frame(ctx, stackframe);
                        MARA_VM_LOAD_STATE(vm);
                        *sp = stack_top = result_copy;
                    }
                } else {
                    MARA_VM_SAVE_STATE(vm);
                    return mara_errorf(
                        ctx,
                        mara_str_from_literal("core/unexpected-type"),
                        "Expecting function got %s",
                        mara_nil(),
                        mara_value_type_name(mara_value_type(stack_top, NULL))
                    );
                }
            }
        MARA_END_OP()
        MARA_BEGIN_OP(RETURN)
            {
                mara_stack_frame_t* stackframe = fp;
                mara_zone_bookmark_t* zone_bookmark = stackframe->zone_bookmark;
                mara_value_t result_copy = mara_copy(
                    ctx,
                    stackframe->return_zone,
                    stack_top
                );

                // Load previous state
                mara_vm_state_t* saved_state = &stackframe->saved_state;
                MARA_VM_LOAD_STATE(saved_state);
                // Rollback zone changes
                while (ctx->current_zone_bookmark != zone_bookmark) {
                    mara_zone_exit(ctx);
                }
                // Destroy the stackframe
                mara_arena_restore(ctx->env, &ctx->control_arena, stackframe->control_snapshot);

                if (fp->closure != NULL) {
                    MARA_VM_DERIVE_STATE();
                    *sp = stack_top = result_copy;
                } else {
                    // Native stack frame
                    MARA_VM_SAVE_STATE(vm);
                    *result = result_copy;
                    return NULL;
                }
            }
        MARA_END_OP()
        MARA_BEGIN_OP(JUMP)
            ip += (int16_t)(operands & 0xffff);
        MARA_END_OP()
        MARA_BEGIN_OP(JUMP_IF_FALSE)
            if (
                mara_value_is_nil(stack_top)
                || mara_value_is_false(stack_top)
            ) {
                ip += (int16_t)(operands & 0xffff);
            }
            stack_top = *(--sp);
        MARA_END_OP()
        MARA_BEGIN_OP(MAKE_CLOSURE)
            {
                // By loading num_captures from the instruction, we avoid
                // loading the function just to read that info
                mara_index_t function_index = (uint8_t)((operands >> 16) & 0xff);
                mara_index_t num_captures = (uint16_t)(operands & 0xffff);
                mara_obj_t* new_obj = mara_alloc_obj(
                    ctx, ctx->current_zone,
                    sizeof(mara_vm_closure_t) + sizeof(mara_value_t) * num_captures
                );
                new_obj->type = MARA_OBJ_TYPE_VM_CLOSURE;
                mara_vm_closure_t* new_closure = (mara_vm_closure_t*)new_obj->body;
                new_closure->fn = functions[function_index];
                for (mara_index_t i = 0; i < num_captures; ++i) {
                    mara_instruction_t capture_instruction = ip[i];
                    mara_opcode_t capture_opcode;
                    mara_operand_t capture_operand;
                    mara_decode_instruction(capture_instruction, &capture_opcode, &capture_operand);

                    mara_value_t captured_value = mara_nil();
                    switch (capture_opcode) {
                        case MARA_OP_GET_ARG:
                            captured_value = args[capture_operand];
                            break;
                        case MARA_OP_GET_LOCAL:
                            captured_value = locals[capture_operand];
                            break;
                        case MARA_OP_GET_CAPTURE:
                            captured_value = captures[capture_operand];
                            break;
                        default:
                            mara_assert(false, "Illegal closure pseudo instruction");
                            break;
                    }
                    mara_obj_add_arena_mask(new_obj, captured_value);
                    new_closure->captures[i] = captured_value;
                }

                *(++sp) = stack_top = mara_obj_to_value(new_obj);
                ip += num_captures;
            }
        MARA_END_OP()
#if 0
        default:
            MARA_VM_SAVE_STATE(vm);
            return mara_errorf(
                ctx,
                mara_str_from_literal("core/illegal-instruction"),
                "Illegal instruction encountered",
                mara_value_from_int(instruction)
            );
#endif
    MARA_END_DISPATCH()
    MARA_WARNING_POP()
}
