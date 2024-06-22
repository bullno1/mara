#include "internal.h"
#include "vm.h"

mara_stack_frame_t*
mara_vm_alloc_stack_frame(mara_exec_ctx_t* ctx, mara_vm_closure_t* closure, mara_vm_state_t vm_state) {
	mara_arena_snapshot_t control_snapshot = mara_arena_snapshot(ctx->env, &ctx->control_arena);
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
		stackframe->native_debug_info = (mara_source_info_t){
			.filename = mara_str_from_literal("<native>")
		};
	}

	stackframe->saved_state = vm_state;
	stackframe->zone_bookmark = ctx->current_zone_bookmark;
	stackframe->control_snapshot = control_snapshot;
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
	mara_assert(check_stackframe == vm->fp, "Unmatched stackframe");
	*vm = stackframe->saved_state;

	mara_zone_bookmark_t* bookmark = stackframe->zone_bookmark;
	while (ctx->current_zone_bookmark != bookmark) {
		mara_zone_exit(ctx);
	}
	mara_arena_restore(ctx->env, &ctx->control_arena, stackframe->control_snapshot);
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
	mara_source_info_t debug_info = ctx->current_zone->debug_info;

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
		mara_zone_enter_new(ctx, (mara_zone_options_t){
			.argc = argc,
			.argv = argv,
		});

		mara_value_t return_value = mara_nil();
		mara_native_closure_t* closure = (mara_native_closure_t*)obj->body;
		error = closure->fn(ctx, argc, argv, closure->userdata, &return_value);
		if (error == NULL) {
			// Copy in case the function allocates in its local zone
			*result = mara_copy(ctx, zone, return_value);
		}
	} else {
		mara_vm_closure_t* closure = (mara_vm_closure_t*)obj->body;
		mara_vm_function_t* mara_fn = closure->fn;
		if (MARA_EXPECT(mara_fn->num_args <= argc)) {
			mara_vm_push_stack_frame(ctx, closure);

			mara_zone_enter_new(ctx, (mara_zone_options_t){
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
