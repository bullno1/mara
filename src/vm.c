#include "internal.h"
#include "vm.h"
#include "vm_intrinsics.h"

MARA_PRIVATE mara_error_t*
mara_vm_execute(mara_exec_ctx_t* ctx, mara_value_t* result);

#include "vendor/nanbox.h"
MARA_PRIVATE bool
mara_value_is_obj(mara_value_t value) {
	nanbox_t nanbox = { .as_int64 = value.internal };
	return nanbox_is_pointer(nanbox);
}

MARA_PRIVATE void*
mara_value_to_ptr(mara_value_t value) {
	nanbox_t nanbox = { .as_int64 = value.internal };
	return nanbox_to_pointer(nanbox);
}

MARA_PRIVATE mara_stack_frame_t*
mara_vm_alloc_stack_frame(mara_exec_ctx_t* ctx, mara_vm_closure_t* closure, mara_vm_state_t vm_state) {
	mara_stack_frame_t* stackframe;
	if (closure != NULL) {
		mara_index_t stack_size = closure->fn->stack_size + 1;  // +1 for sentinel
		stackframe = &ctx->stack_frames[ctx->num_stack_frames++];
		stackframe->closure = closure;
		stackframe->stack = ctx->stack;
		ctx->stack += stack_size;
		stackframe->stack[closure->fn->num_locals] = mara_tombstone();
	} else {
		stackframe = &ctx->stack_frames[ctx->num_stack_frames++];
		stackframe->closure = NULL;
		static mara_source_info_t default_debug_info = {
			.filename = mara_str_from_literal("<native>"),
		};
		stackframe->native_debug_info = &default_debug_info;
	}

	stackframe->saved_state = vm_state;
	stackframe->zone_bookmark = ctx->current_zone_bookmark;
	stackframe->return_zone = ctx->current_zone;
	return stackframe;
}

MARA_PRIVATE mara_stack_frame_t*
mara_vm_push_stack_frame(mara_exec_ctx_t* ctx, mara_vm_closure_t* closure) {
	mara_stack_frame_t* stackframe = mara_vm_alloc_stack_frame(ctx, closure, ctx->vm_state);
	mara_vm_state_t* vm = &ctx->vm_state;
	vm->fp = stackframe;
	vm->sp = closure != NULL ? stackframe->stack + closure->fn->num_locals: NULL;
	vm->ip = closure != NULL ? closure->fn->instructions : NULL;
	return stackframe;
}

void
mara_vm_pop_stack_frame(mara_exec_ctx_t* ctx, mara_stack_frame_t* check_stackframe) {
	(void)check_stackframe;
	mara_vm_state_t* vm = &ctx->vm_state;
	mara_stack_frame_t* stackframe = vm->fp;
    mara_index_t stack_size = stackframe->closure != NULL ? stackframe->closure->fn->stack_size + 1 : 0;
	mara_assert(check_stackframe == vm->fp, "Unmatched stackframe");
	*vm = stackframe->saved_state;

	mara_zone_bookmark_t* bookmark = stackframe->zone_bookmark;
	while (ctx->current_zone_bookmark != bookmark) {
		mara_zone_exit(ctx);
	}
	ctx->num_stack_frames -= 1;
    ctx->stack -= stack_size != 0 ? stack_size + 1 : stack_size;
}

mara_error_t*
mara_apply(
	mara_exec_ctx_t* ctx,
	mara_zone_t* zone,
	mara_fn_t* fn,
	mara_list_t* args,
	mara_value_t* result
) {
	return mara_call(ctx, zone, fn, args->len, args->elems, result);
}

mara_error_t*
mara_call(
	mara_exec_ctx_t* ctx,
	mara_zone_t* zone,
	mara_fn_t* fn,
	mara_index_t argc,
	mara_value_t* argv,
	mara_value_t* result
) {
	mara_obj_t* obj = (mara_obj_t*)fn;
	mara_assert(
		obj->type == MARA_OBJ_TYPE_VM_CLOSURE
		|| obj->type == MARA_OBJ_TYPE_NATIVE_CLOSURE,
		"Invalid object type"
	);
	const mara_source_info_t* debug_info = ctx->current_zone->debug_info;

	// This stack frame ensures that the VM will return here.
	// It will also undo all the zone changes when it's popped.
	mara_stack_frame_t* stackframe = mara_vm_push_stack_frame(ctx, NULL);
	stackframe->native_debug_info = debug_info;

	// Change the return zone if needed
	if (zone != ctx->current_zone) {
		mara_zone_enter(ctx, zone);
	}

	mara_error_t* error = NULL;
	if (obj->type == MARA_OBJ_TYPE_NATIVE_CLOSURE) {
		mara_native_closure_t* closure = (mara_native_closure_t*)obj->body;
		if (!closure->options.no_alloc) {
			mara_zone_enter_new(ctx, &(mara_zone_options_t){
				.argc = argc,
				.argv = argv,
			});
		}

		mara_value_t return_value = mara_nil();
		error = closure->fn(ctx, argc, argv, closure->options.userdata, &return_value);
		if (error == NULL) {
			// Copy in case the function allocates in its local zone
			*result = mara_copy(ctx, zone, return_value);
		}
	} else {
		mara_vm_closure_t* closure = (mara_vm_closure_t*)obj->body;
		mara_vm_function_t* mara_fn = closure->fn;
		if (MARA_EXPECT(mara_fn->num_args <= argc)) {
			mara_vm_push_stack_frame(ctx, closure);

			mara_zone_enter_new(ctx, &(mara_zone_options_t){
				.argc = argc,
				.argv = argv,
			});

			ctx->vm_state.args = argv;
			// The VM always copy the result into the return zone
			error = mara_vm_execute(ctx, result);
		} else {
			error = mara_errorf(
				ctx, mara_str_from_literal("core/wrong-arity"),
				"Function expects %d arguments, got %d",
				mara_nil(),
				mara_fn->num_args, argc
			);
		}
	}

	mara_vm_pop_stack_frame(ctx, stackframe);

	return error;
}

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
#   define MARA_BEGIN_OP(NAME) MARA_OP_##NAME: {
#   define MARA_END_OP() MARA_DISPATCH_NEXT() }
#   define MARA_END_DISPATCH()
#	define MARA_DISPATCH_OP(NAME, OPERANDS) \
		do { \
			operands = OPERANDS; \
			goto MARA_OP_##NAME; \
		} while (0)
#else
#   define MARA_BEGIN_DISPATCH(OPCODE) \
        while (true) { \
            mara_assert(sp <= fp->stack + closure->fn->stack_size, "Stack overflow"); \
            mara_assert((fp->stack + closure->fn->num_locals) <= sp, "Stack underflow"); \
            mara_instruction_t instruction = *ip; \
            ++ip; \
            mara_decode_instruction(instruction, &opcode, &operands); \
            switch (opcode) {
#   define MARA_BEGIN_OP(NAME) case MARA_OP_##NAME: {
#   define MARA_END_OP() } continue;
#   define MARA_END_DISPATCH() }}
#	define MARA_DISPATCH_OP(NAME, OPERANDS) \
		do { \
			operands = OPERANDS; \
			goto MARA_OP_##NAME; \
		} while (0)
#endif

MARA_PRIVATE mara_error_t*
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
		closure = fp->closure; \
		function = closure->fn; \
		closure_header = mara_container_of(closure, mara_obj_t, body); \
	} while (0)

	mara_instruction_t* ip;
	mara_stack_frame_t* fp;
	mara_value_t* sp;
	mara_value_t* args;
	mara_error_t* error;

	mara_vm_closure_t* closure;
	mara_vm_function_t* function;
	mara_obj_t* closure_header;
	// Cache the stack top in a local var.
	// This has been shown through profiling to improve performance.
	mara_value_t stack_top = mara_tombstone();

	mara_vm_state_t* vm = &ctx->vm_state;
	MARA_VM_LOAD_STATE(vm);
	MARA_VM_DERIVE_STATE();

    mara_opcode_t opcode;
    mara_operand_t operands;

    MARA_WARNING_PUSH()
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wgnu-label-as-value"
#endif
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
            *(++sp) = stack_top = mara_copy(ctx, ctx->current_zone, function->constants[operands]);
        MARA_END_OP()
        MARA_BEGIN_OP(POP)
            sp -= operands;
            stack_top = *sp;
        MARA_END_OP()
        MARA_BEGIN_OP(SET_LOCAL)
            fp->stack[operands] = stack_top;
        MARA_END_OP()
        MARA_BEGIN_OP(GET_LOCAL)
            *(++sp) = stack_top = fp->stack[operands];
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
			mara_value_t value_copy = mara_copy(ctx, closure_header->zone, stack_top);
			closure->captures[operands] = value_copy;
			mara_obj_add_arena_mask(closure_header, value_copy);
        MARA_END_OP()
        MARA_BEGIN_OP(GET_CAPTURE)
            *(++sp) = stack_top = closure->captures[operands];
        MARA_END_OP()
        MARA_BEGIN_OP(CALL)
			sp -= operands;

			if (MARA_EXPECT(mara_value_is_obj(stack_top))) {
				mara_obj_t* fn = mara_value_to_ptr(stack_top);
				if (fn->type == MARA_OBJ_TYPE_VM_CLOSURE) {
					mara_vm_closure_t* next_closure = (mara_vm_closure_t*)fn->body;
					if (MARA_EXPECT(next_closure->fn->num_args <= (mara_index_t)operands)) {
						mara_vm_state_t frame_state;
						MARA_VM_SAVE_STATE(&frame_state);
						mara_stack_frame_t* stackframe = mara_vm_alloc_stack_frame(
							ctx, next_closure, frame_state
						);
						mara_zone_enter_new(
							ctx, &(mara_zone_options_t){
								.argc = operands,
								.argv = sp,
							}
						);

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
				} else if (fn->type == MARA_OBJ_TYPE_NATIVE_CLOSURE) {
					mara_native_closure_t* native_closure = (mara_native_closure_t*)fn->body;
					mara_zone_t* return_zone = ctx->current_zone;
					mara_vm_state_t frame_state;
					MARA_VM_SAVE_STATE(&frame_state);
					mara_stack_frame_t* stackframe = mara_vm_alloc_stack_frame(ctx, NULL, frame_state);
					if (!native_closure->options.no_alloc) {
						mara_zone_enter_new(
							ctx, &(mara_zone_options_t){
								.argc = operands,
								.argv = sp,
							}
						);
					}

					args = sp;
					fp = stackframe;
					sp = NULL;
					ip = NULL;
					MARA_VM_SAVE_STATE(vm);

					mara_value_t result = mara_nil();
					error = native_closure->fn(
						ctx,
						operands, args,
						native_closure->options.userdata,
						&result
					);
					if (MARA_EXPECT(error == NULL)) {
						mara_value_t result_copy = mara_copy(ctx, return_zone, result);

						mara_vm_pop_stack_frame(ctx, stackframe);
						MARA_VM_LOAD_STATE(vm);
						*sp = stack_top = result_copy;
					} else {
						return error;
					}
				} else {
					goto invalid_type;
				}
			} else {
				goto invalid_type;
			}
        MARA_END_OP()
        MARA_BEGIN_OP(RETURN)
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
			ctx->num_stack_frames -= 1;
			mara_index_t stack_size = stackframe->closure != NULL ? stackframe->closure->fn->stack_size + 1 : 0;
			ctx->stack -= stack_size;

			if (fp->closure != NULL) {
				MARA_VM_DERIVE_STATE();
				*sp = stack_top = result_copy;
			} else {
				// Native stack frame
				MARA_VM_SAVE_STATE(vm);
				*result = result_copy;
				return NULL;
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
			new_closure->fn = function->functions[function_index];
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
						captured_value = fp->stack[capture_operand];
						break;
					case MARA_OP_GET_CAPTURE:
						captured_value = closure->captures[capture_operand];
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
        MARA_END_OP()
		// Intrinsics
		MARA_BEGIN_OP(LT)
			sp -= 1;
			if (MARA_EXPECT((error = mara_intrin_lt(ctx, 2, sp, NULL, &stack_top)) == NULL)) {
				*sp = stack_top;
			} else {
				MARA_VM_SAVE_STATE(vm);
				return error;
			}
        MARA_END_OP()
        MARA_BEGIN_OP(LTE)
			sp -= 1;
			if (MARA_EXPECT((error = mara_intrin_lte(ctx, 2, sp, NULL, &stack_top)) == NULL)) {
				*sp = stack_top;
			} else {
				MARA_VM_SAVE_STATE(vm);
				return error;
			}
        MARA_END_OP()
        MARA_BEGIN_OP(GT)
			sp -= 1;
			if (MARA_EXPECT((error = mara_intrin_gt(ctx, 2, sp, NULL, &stack_top)) == NULL)) {
				*sp = stack_top;
			} else {
				MARA_VM_SAVE_STATE(vm);
				return error;
			}
        MARA_END_OP()
        MARA_BEGIN_OP(GTE)
			sp -= 1;
			if (MARA_EXPECT((error = mara_intrin_gte(ctx, 2, sp, NULL, &stack_top)) == NULL)) {
				*sp = stack_top;
			} else {
				MARA_VM_SAVE_STATE(vm);
				return error;
			}
        MARA_END_OP()
		// Super instructions
        MARA_BEGIN_OP(CALL_CAPTURE)
			mara_operand_t capture_index = operands & 0xffff;
			mara_operand_t num_args = (operands >> 16) & 0xff;
            *(++sp) = stack_top = closure->captures[capture_index];
			MARA_DISPATCH_OP(CALL, num_args);
        MARA_END_OP()
invalid_type:
			MARA_VM_SAVE_STATE(vm);
			return mara_errorf(
				ctx,
				mara_str_from_literal("core/unexpected-type"),
				"Expecting function got %s",
				mara_nil(),
				mara_value_type_name(mara_value_type(stack_top, NULL))
			);
		// TODO: safety checks
		// * Illegal instruction
		// * Stack over/under flow when creating frames
		// *                       when executing to catch miscompilation
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
