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

	mara_value_t extra_copy;
	mara_copy(ctx, &ctx->error_zone, extra, &extra_copy),
	ctx->last_error = (mara_error_t){
		.type = {
			.len = type.len,
			.data = type_str,
		},
		.message = mara_vsnprintf(ctx, &ctx->error_zone, fmt, args),
		.extra = extra_copy,
	};
	if (
		ctx->vm_state.fp == NULL
		&& ctx->current_zone->debug_info.filename.data != NULL
	) {
		mara_stacktrace_t* stacktrace = mara_zone_alloc_ex(
			ctx,
			&ctx->error_zone,
			sizeof(mara_stacktrace_t) + sizeof(mara_source_info_t),
			sizeof(mara_stacktrace_t)
		);
		stacktrace->len = 1;
		stacktrace->frames[0] = ctx->current_zone->debug_info;

		ctx->last_error.stacktrace = stacktrace;
	}

	return &ctx->last_error;
}
