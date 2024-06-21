#include "internal.h"
#include "vm.h"

MARA_PRIVATE mara_stack_frame_t*
mara_vm_alloc_stack_frame(mara_exec_ctx_t* ctx, mara_obj_closure_t* closure, mara_vm_state_t vm_state) {
	mara_stack_frame_t* stackframe;
	if (closure != NULL) {
		mara_index_t stack_size = closure->fn->stack_size + 1;  // +1 for sentinel
		stackframe = mara_arena_alloc_ex(
			ctx->env, &ctx->control_arena,
			sizeof(mara_stack_frame_t) + sizeof(mara_value_t) * stack_size,
			_Alignof(mara_stack_frame_t)
		);
		stackframe->closure = closure;
		stackframe->stack[closure->fn->num_locals] = mara_tombstone();
	} else {
		stackframe = mara_arena_alloc_ex(
			ctx->env, &ctx->control_arena,
			sizeof(mara_stack_frame_t), _Alignof(mara_stack_frame_t)
		);
		stackframe->closure = NULL;
	}

	stackframe->saved_state = vm_state;
	stackframe->zone_bookmark = ctx->current_zone_bookmark;
	return stackframe;
}

MARA_PRIVATE void
mara_vm_push_stack_frame(mara_exec_ctx_t* ctx, mara_obj_closure_t* closure) {
	mara_stack_frame_t* stackframe = mara_vm_alloc_stack_frame(ctx, closure, ctx->vm_state);
	mara_vm_state_t* vm = &ctx->vm_state;
	vm->fp = stackframe;
	vm->sp = closure != NULL ? stackframe->stack + closure->fn->num_locals: NULL;
	vm->ip = closure != NULL ? closure->fn->instructions : NULL;
}

MARA_PRIVATE void
mara_vm_pop_stack_frame(mara_exec_ctx_t* ctx) {
	mara_vm_state_t* vm = &ctx->vm_state;
	mara_stack_frame_t* stackframe = vm->fp;
	*vm = stackframe->saved_state;

	mara_zone_bookmark_t* bookmark = stackframe->zone_bookmark;
	while (ctx->current_zone_bookmark != bookmark) {
		mara_zone_exit(ctx);
	}
}

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

	mara_obj_closure_t* closure;
	mara_value_t* captures;
	mara_value_t* locals;
	mara_value_t* constants;
	mara_function_t** functions;
	mara_obj_t* closure_header;
	mara_value_t stack_top;

	mara_vm_state_t* vm = &ctx->vm_state;
	MARA_VM_LOAD_STATE(vm);
	MARA_VM_DERIVE_STATE();

	while (true) {
		mara_instruction_t instruction = (*ip)++;
		mara_opcode_t opcode;
		mara_operand_t operands;
		mara_decode_instruction(instruction, &opcode, &operands);

		// TODO: computed goto and switched goto
		switch (opcode) {
			case MARA_OP_NOP:
				continue;
			case MARA_OP_NIL:
				*(++sp) = stack_top = mara_nil();
				continue;
			case MARA_OP_TRUE:
				*(++sp) = stack_top = mara_value_from_bool(true);
				continue;
			case MARA_OP_FALSE:
				*(++sp) = stack_top = mara_value_from_bool(false);
				continue;
			case MARA_OP_SMALL_INT:
				*(++sp) = stack_top = mara_value_from_int((uint16_t)(operands & 0xffff));
				continue;
			case MARA_OP_CONSTANT:
				// Constants come from permanent zone so copying is needed
				mara_copy(ctx, ctx->current_zone, constants[operands], &stack_top);
				*(++sp) = stack_top;
				continue;
			case MARA_OP_POP:
				sp -= operands;
				stack_top = *sp;
				continue;
			case MARA_OP_SET_LOCAL:
				locals[operands] = stack_top;
				continue;
			case MARA_OP_GET_LOCAL:
				*(++sp) = stack_top = locals[operands];
				continue;
			case MARA_OP_SET_ARG:
				// TODO: Rethink this opcode
				// The arguments may come from a list in mara_apply
				// Some zone crossing might happen.
				args[operands] = stack_top;
				continue;
			case MARA_OP_GET_ARG:
				*(++sp) = stack_top = args[operands];
				continue;
			case MARA_OP_SET_CAPTURE:
				mara_copy(ctx, closure_header->zone, stack_top, &captures[operands]);
				mara_obj_add_arena_mask(closure_header, stack_top);
				continue;
			case MARA_OP_GET_CAPTURE:
				*(++sp) = stack_top = captures[operands];
				continue;
			case MARA_OP_CALL:
				{
					mara_obj_t* fn = mara_value_to_obj(stack_top);
					if (
						MARA_EXPECT(
							fn != NULL
							&& (
								fn->type == MARA_OBJ_TYPE_MARA_FN
								|| fn->type == MARA_OBJ_TYPE_NATIVE_FN
							)
						)
					) {
						mara_obj_closure_t* next_closure;
						if (fn->type == MARA_OBJ_TYPE_MARA_FN) {
							next_closure = (mara_obj_closure_t*)fn->body;
						} else {
							next_closure = NULL;
						}

						sp -= operands;
						mara_zone_t* return_zone = ctx->current_zone;

						mara_zone_enter_new(
							ctx, (mara_zone_options_t){
								.argc = operands,
								.argv = sp,
							}
						);

						mara_vm_state_t frame_state;
						MARA_VM_SAVE_STATE(&frame_state);
						mara_stack_frame_t* stackframe = mara_vm_alloc_stack_frame(
							ctx, next_closure, frame_state
						);
						args = sp;
						fp = stackframe;

						if(next_closure != NULL) {
							sp = stackframe->stack + next_closure->fn->num_locals;
							ip = next_closure->fn->instructions;
							MARA_VM_DERIVE_STATE();
						} else {
							sp = NULL;
							ip = NULL;
							MARA_VM_SAVE_STATE(vm);

							mara_native_fn_t* native_fn = (mara_native_fn_t*)fn->body;
							mara_value_t result;
							mara_check_error(
								native_fn->fn(ctx, operands, args, native_fn->userdata, &result)
							);
							mara_value_t result_copy;
							mara_copy(ctx, return_zone, result, &result_copy);

							mara_vm_pop_stack_frame(ctx);
							mara_zone_exit(ctx);
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
				continue;
			case MARA_OP_RETURN:
				{
					mara_zone_bookmark_t* zone_bookmark = fp->zone_bookmark;
					mara_value_t result_copy;
					mara_copy(
						ctx,
						// Stackframe is created inside the new zone
						zone_bookmark->previous_zone,
						stack_top,
						&result_copy
					);

					mara_vm_state_t* saved_state = &fp->saved_state;
					mara_stack_frame_t* previous_stack_frame = saved_state->fp;
					if (previous_stack_frame->closure == NULL) {
						*result = result_copy;
						mara_vm_pop_stack_frame(ctx);
						mara_zone_exit(ctx);
						return NULL;
					} else {
						MARA_VM_LOAD_STATE(saved_state);
						MARA_VM_DERIVE_STATE();
						*sp = stack_top = result_copy;

						while (ctx->current_zone_bookmark != zone_bookmark) {
							mara_zone_exit(ctx);
						}
						mara_zone_exit(ctx);
					}
				}
				continue;
			case MARA_OP_JUMP:
				ip += (int16_t)(operands & 0xffff);
				continue;
			case MARA_OP_JUMP_IF_FALSE:
				if (
					mara_value_is_nil(stack_top)
					|| mara_value_is_false(stack_top)
				) {
					ip += (int16_t)(operands & 0xffff);
				}
				stack_top = *(--sp);
				continue;
			case MARA_OP_MAKE_CLOSURE:
				{
					// By loading num_captures from the instruction, we avoid
					// loading the function just to read that info
					mara_index_t function_index = (uint8_t)((operands >> 16) & 0xff);
					mara_index_t num_captures = (uint16_t)(operands & 0xffff);
					mara_obj_t* new_obj = mara_alloc_obj(
						ctx, ctx->current_zone,
						sizeof(mara_obj_closure_t) + sizeof(mara_value_t) * num_captures
					);
					new_obj->type = MARA_OBJ_TYPE_MARA_FN;
					mara_obj_closure_t* new_closure = (mara_obj_closure_t*)new_obj->body;
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
				continue;
			default:
				MARA_VM_SAVE_STATE(vm);
				return mara_errorf(
					ctx,
					mara_str_from_literal("core/illegal-instruction"),
					"Illegal instruction encountered",
					mara_value_from_int(instruction)
				);
		}
	}
}

mara_error_t*
mara_apply(
	mara_exec_ctx_t* ctx,
	mara_zone_t* zone,
	mara_value_t fn,
	mara_value_t args,
	mara_value_t* result
) {
	mara_obj_list_t* arg_list;
	mara_check_error(mara_unbox_list(ctx, args, &arg_list));

	return mara_call(ctx, zone, fn, arg_list->len, arg_list->elems, result);
}

mara_error_t*
mara_call(
	mara_exec_ctx_t* ctx,
	mara_zone_t* zone,
	mara_value_t fn,
	mara_index_t argc,
	mara_value_t* argv,
	mara_value_t* result
) {
	mara_obj_t* obj = mara_value_to_obj(fn);
	if (
		MARA_EXPECT(
			obj != NULL
			&& (
				obj->type == MARA_OBJ_TYPE_MARA_FN
				|| obj->type == MARA_OBJ_TYPE_NATIVE_FN
			)
		)
	) {
		// Change the return zone if needed
		bool switch_return_zone = zone != ctx->current_zone;
		if (switch_return_zone) {
			mara_zone_enter(ctx, zone);
		}

		// Enter new zone for function body
		mara_zone_enter_new(ctx, (mara_zone_options_t){
			.argc = argc,
			.argv = argv,
		});

		mara_vm_push_stack_frame(ctx, NULL);

		mara_error_t* error = NULL;
		if (obj->type == MARA_OBJ_TYPE_NATIVE_FN) {
			mara_native_fn_t* native_fn = (mara_native_fn_t*)obj->body;
			error = native_fn->fn(ctx, argc, argv, native_fn->userdata, result);
		} else {
			mara_obj_closure_t* closure = (mara_obj_closure_t*)obj->body;
			mara_function_t* mara_fn = closure->fn;
			if (MARA_EXPECT(mara_fn->num_args <= argc)) {
				mara_vm_push_stack_frame(ctx, closure);
				ctx->vm_state.args = argv;
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

		mara_vm_pop_stack_frame(ctx);
		mara_zone_exit(ctx);
		if (switch_return_zone) {
			mara_zone_exit(ctx);
		}

		return error;
	} else {
		return mara_errorf(
			ctx,
			mara_str_from_literal("core/unexpected-type"),
			"Expecting function got %s",
			mara_nil(),
			mara_value_type_name(mara_value_type(fn, NULL))
		);
	}
}
