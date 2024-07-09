#include "internal.h"
#include "vm.h"
#include "mara/utils.h"
#include "vm_intrinsics.h"

MARA_PRIVATE mara_error_t*
mara_vm_execute(mara_exec_ctx_t* ctx, mara_value_t* result);

MARA_PRIVATE mara_stack_frame_t*
mara_vm_alloc_stack_frame(
	const mara_exec_ctx_t* ctx,
	const mara_vm_state_t* vm_state,
	mara_vm_closure_t* vm_closure,
	mara_zone_t* return_zone
) {
	mara_stack_frame_t* current_stack_frame = vm_state->fp;
	mara_stack_frame_t* new_stack_frame = current_stack_frame + 1;
	if (MARA_EXPECT(new_stack_frame < ctx->stack_frames_end)) {
		mara_index_t current_frame_size = current_stack_frame->vm_closure != NULL
			? current_stack_frame->vm_closure->fn->stack_size + 1  // For sentinel
			: 0;
		mara_index_t new_frame_size = vm_closure != NULL
			? vm_closure->fn->stack_size + 1  // For sentinel
			: 0;
		mara_value_t* stack = current_stack_frame->stack + current_frame_size;
		if (MARA_EXPECT(stack + new_frame_size <= ctx->stack_end)) {
			*new_stack_frame = (mara_stack_frame_t){
				.previous_vm_state = *vm_state,
				.return_zone = return_zone,
				.stack = stack,
				.vm_closure = vm_closure,
			};

			return new_stack_frame;
		} else {
			return NULL;
		}
	} else {
		return NULL;
	}
}

MARA_PRIVATE void
mara_vm_pop_stack_frame(mara_exec_ctx_t* ctx, mara_stack_frame_t* stack_frame) {
	mara_vm_state_t* vm = &ctx->vm_state;
	mara_assert(stack_frame == vm->fp, "Unmatched stack frame");
	*vm = stack_frame->previous_vm_state;
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
	mara_error_t* error = NULL;
	mara_vm_state_t* vm_state = &ctx->vm_state;

	if (obj->type == MARA_OBJ_TYPE_NATIVE_CLOSURE) {
		mara_native_closure_t* closure = (mara_native_closure_t*)obj->body;
		mara_zone_t* call_zone = NULL;
		if (!closure->no_alloc) {
			call_zone = mara_zone_enter(ctx, (mara_zone_options_t){
				.argc = argc,
				.argv = argv,
				.return_zone = zone,
			});
			if (call_zone == NULL) {
				return mara_errorf(
					ctx,
					mara_str_from_literal("core/limit-reached/stack-overflow"),
					"Too many stack frames",
					mara_nil()
				);
			}
		}

		mara_value_t return_value = mara_nil();
		mara_zone_t* previous_return_zone = vm_state->fp->return_zone;
		vm_state->fp->return_zone = zone;
		error = closure->fn(ctx, argc, argv, closure->userdata, &return_value);
		vm_state->fp->return_zone = previous_return_zone;
		if (MARA_EXPECT(error == NULL)) {
			// Copy in case the function allocates in its local zone
			*result = mara_copy(ctx, zone, return_value);
		}

		if (call_zone != NULL) {
			mara_zone_exit(ctx, call_zone);
		}
	} else {
		mara_vm_closure_t* closure = (mara_vm_closure_t*)obj->body;
		mara_vm_function_t* mara_fn = closure->fn;
		if (MARA_EXPECT(mara_fn->num_args <= argc)) {
			mara_stack_frame_t* stack_frame = mara_vm_alloc_stack_frame(ctx, vm_state, closure, zone);
			if (MARA_EXPECT(stack_frame != NULL)) {
				stack_frame->stack[0] = mara_tombstone();
				vm_state->fp = stack_frame;
				vm_state->args = argv;
				vm_state->sp = stack_frame->stack + mara_fn->num_locals;
				vm_state->ip = mara_fn->instructions;

				mara_zone_t* call_zone = mara_zone_enter(ctx, (mara_zone_options_t){
					.argc = argc,
					.argv = argv,
					.return_zone = zone,
					.vm_closure = closure,
				});
				// There are as many zones as stack frames
				(void)call_zone;
				mara_assert(call_zone != NULL, "Cannot alloc call zone");

				// The VM always copy the result into the return zone
				error = mara_vm_execute(ctx, result);
				// call_zone will be cleaned up by the VM
			} else {
				error = mara_errorf(
					ctx, mara_str_from_literal("core/stack-overflow"),
					"Stack overflow",
					mara_nil()
				);
			}
		} else {
			error = mara_errorf(
				ctx, mara_str_from_literal("core/wrong-arity"),
				"Function expects %d arguments, got %d",
				mara_nil(),
				mara_fn->num_args, argc
			);
		}
	}

	return error;
}

// VM dispatch loop
// It has to be here so that certain functions are inlined

#if defined(__GNUC__) || defined(__clang__)
// Computed goto
#	define MARA_DISPATCH_ENTRY(X) &&MARA_OP_##X,
#	define MARA_DISPATCH_NEXT() \
		{ \
			mara_instruction_t instruction = *ip; \
			++ip; \
			mara_decode_instruction(instruction, &opcode, &operands); \
		} \
		goto *dispatch_table[opcode];
#	define MARA_BEGIN_DISPATCH() \
		static void* dispatch_table[] = { \
			MARA_OPCODE(MARA_DISPATCH_ENTRY) \
		}; \
		MARA_DISPATCH_NEXT()
#	define MARA_BEGIN_OP(NAME) MARA_OP_##NAME: {
#	define MARA_END_OP() } MARA_DISPATCH_NEXT()
#	define MARA_END_DISPATCH()
#	define MARA_BEGIN_SUBROUTINE(NAME) NAME: {
#	define MARA_END_SUBROUTINE(NAME) } MARA_DISPATCH_NEXT()
#	define MARA_CALL_SUBROUTINE(NAME) goto NAME;
#elif 1
// Switched goto
#	define MARA_DISPATCH_ENTRY(X) \
		case MARA_OP_##X: goto MARA_OP_##X;
#	define MARA_DISPATCH_NEXT() \
		{ \
			mara_instruction_t instruction = *ip; \
			++ip; \
			mara_decode_instruction(instruction, &opcode, &operands); \
		}; \
		switch (opcode) { \
			MARA_OPCODE(MARA_DISPATCH_ENTRY) \
		}
#	define MARA_BEGIN_DISPATCH() \
		MARA_DISPATCH_NEXT()
#	define MARA_BEGIN_OP(NAME) MARA_OP_##NAME: {
#	define MARA_END_OP() } MARA_DISPATCH_NEXT()
#	define MARA_END_DISPATCH()
#	define MARA_BEGIN_SUBROUTINE(NAME) NAME: {
#	define MARA_END_SUBROUTINE(NAME) } MARA_DISPATCH_NEXT()
#	define MARA_CALL_SUBROUTINE(NAME) goto NAME;
#else
// Switch case
#	define MARA_BEGIN_DISPATCH() \
		while (true) { \
			{ \
				mara_instruction_t instruction = *ip; \
				++ip; \
				mara_decode_instruction(instruction, &opcode, &operands); \
			} \
			redispatch: switch (opcode) {
#	define MARA_BEGIN_OP(NAME) case MARA_OP_##NAME: {
#	define MARA_END_OP() } continue;
#	define MARA_END_DISPATCH() }}
#	define MARA_BEGIN_SUBROUTINE(NAME) MARA_SUBROUTINE_##NAME: {
#	define MARA_END_SUBROUTINE(NAME) } goto redispatch;
#	define MARA_CALL_SUBROUTINE(NAME) goto NAME;
#endif

#define MARA_VM_QUICKEN(NAME) \
	*(ip - 1) = mara_encode_instruction(NAME, operands)

#define MARA_HANDLE_INTRINSIC(NAME) \
	MARA_BEGIN_OP(NAME) \
		if (MARA_EXPECT(mara_value_is_obj(stack_top))) { \
			top_obj = mara_value_to_obj(stack_top); \
			if (MARA_EXPECT(top_obj->quickened_opcode == opcode)) { \
				sp -= operands; \
				if (MARA_EXPECT((error = mara_intrin_##NAME(ctx, operands, sp, mara_nil(), &stack_top)) == NULL)) { \
					*sp = stack_top; \
				} else { \
					goto intrinsic_error; \
				} \
			} else { \
				MARA_VM_QUICKEN(MARA_OP_CALL_GENERIC); \
				goto MARA_OP_CALL_GENERIC; \
			} \
		} else { \
			goto invalid_call_type; \
		} \
	MARA_END_OP()

MARA_WARNING_PUSH()

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wgnu-label-as-value"
#elif defined(_MSC_VER)
// MARA_END_OP is not reachable when following MARA_DISPATCH_OP
// The warning is unnecessary.
// MARA_END_OP is there to serve as a closing block annotation.
// The code will get eliminated anyway.
#pragma warning(disable: 4702)
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
		closure = fp->vm_closure; \
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

	mara_obj_t* top_obj;
	mara_opcode_t opcode;
	mara_operand_t operands;

	MARA_BEGIN_DISPATCH()
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
			if (MARA_EXPECT(mara_value_is_obj(stack_top))) {
				top_obj = mara_value_to_obj(stack_top);
				if (top_obj->type == MARA_OBJ_TYPE_VM_CLOSURE) {
					MARA_VM_QUICKEN(MARA_OP_CALL_VM);
					MARA_CALL_SUBROUTINE(handle_vm_call);
				} else if (top_obj->type == MARA_OBJ_TYPE_NATIVE_CLOSURE) {
					MARA_VM_QUICKEN(top_obj->quickened_opcode);
					MARA_CALL_SUBROUTINE(handle_native_call);
				} else {
					goto invalid_call_type;
				}
			} else {
				goto invalid_call_type;
			}
		MARA_END_OP()
		MARA_BEGIN_OP(CALL_VM)
			if (MARA_EXPECT(mara_value_is_obj(stack_top))) {
				top_obj = mara_value_to_obj(stack_top);
				if (MARA_EXPECT(top_obj->type == MARA_OBJ_TYPE_VM_CLOSURE)) {
					MARA_CALL_SUBROUTINE(handle_vm_call);
				} else if (top_obj->type == MARA_OBJ_TYPE_NATIVE_CLOSURE) {
					MARA_VM_QUICKEN(MARA_OP_CALL_GENERIC);
					MARA_CALL_SUBROUTINE(handle_native_call);
				} else {
					goto invalid_call_type;
				}
			} else {
				goto invalid_call_type;
			}
		MARA_END_OP()
		MARA_BEGIN_OP(CALL_NATIVE)
			if (MARA_EXPECT(mara_value_is_obj(stack_top))) {
				top_obj = mara_value_to_obj(stack_top);
				if (MARA_EXPECT(top_obj->type == MARA_OBJ_TYPE_NATIVE_CLOSURE)) {
					MARA_CALL_SUBROUTINE(handle_native_call);
				} else if (MARA_EXPECT(top_obj->type == MARA_OBJ_TYPE_VM_CLOSURE)) {
					MARA_VM_QUICKEN(MARA_OP_CALL_GENERIC);
					MARA_CALL_SUBROUTINE(handle_vm_call);
				} else {
					goto invalid_call_type;
				}
			} else {
				goto invalid_call_type;
			}
		MARA_END_OP()
		MARA_BEGIN_OP(CALL_GENERIC)
			if (MARA_EXPECT(mara_value_is_obj(stack_top))) {
				top_obj = mara_value_to_obj(stack_top);
				if (top_obj->type == MARA_OBJ_TYPE_VM_CLOSURE) {
					MARA_CALL_SUBROUTINE(handle_vm_call);
				} else if (top_obj->type == MARA_OBJ_TYPE_NATIVE_CLOSURE) {
					MARA_CALL_SUBROUTINE(handle_native_call);
				} else {
					goto invalid_call_type;
				}
			} else {
				goto invalid_call_type;
			}
		MARA_END_OP()
		MARA_BEGIN_OP(RETURN)
			mara_stack_frame_t* stack_frame = fp;
			mara_value_t result_copy = mara_copy(
				ctx,
				stack_frame->return_zone,
				stack_top
			);

			// Load previous state
			mara_vm_state_t* saved_state = &stack_frame->previous_vm_state;
			MARA_VM_LOAD_STATE(saved_state);

			if (fp->vm_closure != NULL) {
				mara_zone_exit(ctx, ctx->current_zone);
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

		MARA_INTRINSIC(MARA_HANDLE_INTRINSIC)
	MARA_END_DISPATCH()

	MARA_BEGIN_SUBROUTINE(handle_vm_call)
		mara_vm_closure_t* next_closure = (mara_vm_closure_t*)top_obj->body;
		if (MARA_EXPECT(next_closure->fn->num_args <= (mara_index_t)operands)) {
			sp -= operands;
			mara_vm_state_t frame_state;
			MARA_VM_SAVE_STATE(&frame_state);
			mara_stack_frame_t* stack_frame = mara_vm_alloc_stack_frame(
				ctx, &frame_state, next_closure, ctx->current_zone
			);
			if (MARA_EXPECT(stack_frame != NULL)) {

				stack_frame->stack[0] = mara_tombstone();
				mara_zone_t* call_zone = mara_zone_enter(
					ctx,
					(mara_zone_options_t){
						.argc = operands,
						.argv = sp,
						.return_zone = ctx->current_zone,
						.vm_closure = next_closure,
					}
				);
				(void)call_zone;
				mara_assert(call_zone != NULL, "Cannot alloc call zone");

				args = sp;
				fp = stack_frame;
				sp = stack_frame->stack + next_closure->fn->num_locals;
				ip = next_closure->fn->instructions;
				MARA_VM_DERIVE_STATE();
			} else {
				MARA_VM_SAVE_STATE(vm);
				return mara_errorf(
					ctx, mara_str_from_literal("core/stack-overflow"),
					"Stack overflow",
					mara_nil()
				);
			}
		} else {
			MARA_VM_SAVE_STATE(vm);
			return mara_errorf(
				ctx, mara_str_from_literal("core/wrong-arity"),
				"Function expects %d arguments, got %d",
				mara_nil(),
				next_closure->fn->num_args, operands
			);
		}
	MARA_END_SUBROUTINE()

	MARA_BEGIN_SUBROUTINE(handle_native_call)
		mara_native_closure_t* native_closure = (mara_native_closure_t*)top_obj->body;

		sp -= operands;
		mara_zone_t* return_zone = ctx->current_zone;
		mara_vm_state_t frame_state;
		MARA_VM_SAVE_STATE(&frame_state);
		mara_stack_frame_t* stack_frame = mara_vm_alloc_stack_frame(
			ctx, &frame_state, NULL, return_zone
		);

		if (MARA_EXPECT(stack_frame != NULL)) {
			ctx->native_debug_info[stack_frame - ctx->stack_frames_begin] = NULL;

			args = sp;
			fp = stack_frame;
			sp = NULL;
			ip = NULL;
			MARA_VM_SAVE_STATE(vm);

			mara_zone_t* call_zone = NULL;
			if (!native_closure->no_alloc) {
				call_zone = mara_zone_enter(
					ctx,
					(mara_zone_options_t){
						.argc = operands,
						.argv = args,
						.return_zone = return_zone,
					}
				);
				mara_assert(call_zone != NULL, "Cannot alloc call zone");
			}

			mara_value_t call_result = mara_nil();
			error = native_closure->fn(
				ctx,
				operands, args,
				native_closure->userdata,
				&call_result
			);
			if (MARA_EXPECT(error == NULL)) {
				mara_value_t result_copy = mara_copy(ctx, return_zone, call_result);

				if (call_zone != NULL) {
					mara_zone_exit(ctx, call_zone);
				}

				mara_vm_pop_stack_frame(ctx, stack_frame);
				MARA_VM_LOAD_STATE(vm);
				*sp = stack_top = result_copy;
			} else {
				if (call_zone != NULL) {
					mara_zone_exit(ctx, call_zone);
				}

				// VM state is already saved before calling the
				// native function
				return error;
			}
		} else {
			MARA_VM_SAVE_STATE(vm);
			return mara_errorf(
				ctx, mara_str_from_literal("core/stack-overflow"),
				"Stack overflow",
				mara_nil()
			);
		}
	MARA_END_SUBROUTINE()

invalid_call_type:
	MARA_VM_SAVE_STATE(vm);
	return mara_errorf(
		ctx,
		mara_str_from_literal("core/unexpected-type"),
		"Expecting function got %s",
		mara_nil(),
		mara_value_type_name(mara_value_type(stack_top, NULL))
	);

intrinsic_error:
	// Rebuild stacktrace with accurate sp and fp
	MARA_VM_SAVE_STATE(vm);
	ctx->last_error.stacktrace = mara_build_stacktrace(ctx);
	return &ctx->last_error;

	// TODO: safety checks
	// * Illegal instruction
	// * Stack over/underflow when executing to catch miscompilation
}

MARA_WARNING_POP()
