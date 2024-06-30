#include "internal.h"

mara_error_t*
mara_errorf(
	mara_exec_ctx_t* ctx,
	mara_str_t type,
	const char* fmt,
	mara_value_t extra,
	...
) {
	va_list args;
	va_start(args, extra);
	mara_error_t* error = mara_errorv(ctx, type, fmt, extra, args);
	va_end(args);
	return error;
}

mara_error_t*
mara_errorv(
	mara_exec_ctx_t* ctx,
	mara_str_t type,
	const char* fmt,
	mara_value_t extra,
	va_list args
) {
	char* type_str = mara_zone_alloc_ex(ctx, &ctx->error_zone, type.len, _Alignof(char));
	memcpy(type_str, type.data, type.len);

	mara_value_t extra_copy = mara_copy(ctx, &ctx->error_zone, extra);
	ctx->last_error = (mara_error_t){
		.type = {
			.len = type.len,
			.data = type_str,
		},
		.message = mara_vsnprintf(ctx, &ctx->error_zone, fmt, args),
		.extra = extra_copy,
	};

	ctx->last_error.stacktrace = mara_build_stacktrace(ctx);

	return &ctx->last_error;
}

mara_stacktrace_t*
mara_build_stacktrace(mara_exec_ctx_t* ctx) {
	mara_index_t num_frames = (mara_index_t)(ctx->vm_state.fp - ctx->stack_frames_begin + 1);
	mara_stacktrace_t* stacktrace = mara_zone_alloc_ex(
		ctx,
		&ctx->error_zone,
		sizeof(mara_stacktrace_t) + sizeof(mara_source_info_t) * num_frames,
		sizeof(mara_stacktrace_t)
	);
	stacktrace->len = num_frames;

	mara_index_t frame_index = 0;
	mara_vm_state_t vm_state = ctx->vm_state;
	for (
		mara_stack_frame_t* itr = ctx->vm_state.fp;
		frame_index < num_frames;
		itr = itr->previous_vm_state.fp, ++frame_index
	) {
		mara_source_info_t* frame = &stacktrace->frames[frame_index];
		mara_vm_closure_t* closure = itr->vm_closure;
		if (closure == NULL) {
			const mara_source_info_t* native_debug_info = ctx->native_debug_info[itr - ctx->stack_frames_begin];
			if (native_debug_info != NULL) {
				*frame = *native_debug_info;
			} else {
				*frame = (mara_source_info_t){
					.filename = mara_str_from_literal("<native>"),
				};
			}
		} else if (closure->fn->source_info != NULL) {
			mara_source_info_t* debug_info = closure->fn->source_info;
			mara_index_t instruction_offset = (mara_index_t)(vm_state.ip - closure->fn->instructions - 1);
			*frame = debug_info[instruction_offset];
		} else {
			*frame = (mara_source_info_t){
				.filename = itr->vm_closure->fn->filename,
			};
		}

		vm_state = itr->previous_vm_state;
	}

	return stacktrace;
}
