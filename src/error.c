#include "internal.h"
#include "mara.h"

mara_error_t*
mara_new_errorf(
	mara_exec_ctx_t* ctx,
	mara_str_t type,
	const char* fmt,
	mara_value_t extra,
	...
) {
	va_list args;
	va_start(args, extra);
	mara_error_t* error = mara_new_errorv(ctx, type, fmt, extra, args);
	va_end(args);
	return error;
}

mara_error_t*
mara_new_errorv(
	mara_exec_ctx_t* ctx,
	mara_str_t type,
	const char* fmt,
	mara_value_t extra,
	va_list args
) {
	// TODO: stacktrace
	mara_error_t* error = mara_zone_alloc(ctx, &ctx->context_zone, sizeof(mara_error_t));

	char* type_str = mara_zone_alloc(ctx, &ctx->context_zone, type.len);
	memcpy(type_str, type.data, type.len);

	*error = (mara_error_t){
		.type = {
			.len = type.len,
			.data = type_str,
		},
		.message = mara_vsnprintf(ctx, &ctx->context_zone, fmt, args),
		.extra = mara_copy(ctx, &ctx->context_zone, extra),
	};

	return error;
}
