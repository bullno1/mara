#include "internal.h"
#include "nanoprintf.h"

MARA_PRIVATE void
mara_putc(int ch, void* ctx) {
	mara_writer_t* writer = ctx;
	mara_write(&ch, 1, *writer);
}

void
mara_set_debug_info(mara_exec_ctx_t* ctx, mara_source_info_t debug_info) {
	ctx->current_zone->debug_info = debug_info;
}

void
mara_print_value(
	mara_exec_ctx_t* ctx,
	mara_value_t value,
	mara_print_options_t options,
	mara_writer_t output
);

void
mara_print_error(
	mara_exec_ctx_t* ctx,
	mara_error_t* error,
	mara_print_options_t options,
	mara_writer_t output
) {
	npf_pprintf(
		mara_putc, &output,
		"%.*s: %.*s\n",
		error->type.len, error->type.data,
		error->message.len, error->message.data
	);
	(void)ctx;
	(void)options;
}
