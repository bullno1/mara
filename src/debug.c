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
	(void)ctx;
	(void)options;

	npf_pprintf(
		mara_putc, &output,
		"┌error:%.*s: %.*s\n",
		error->type.len, error->type.data,
		error->message.len, error->message.data
	);
	for (
		mara_index_t i = 0;
		error->stacktrace != NULL && i < error->stacktrace->len;
		++i
	) {
		mara_source_info_t* src_info = &error->stacktrace->frames[i];
		npf_pprintf(
			mara_putc, &output,
			"%s─%.*s:%d:%d:%d - %d:%d:%d\n",
			i < error->stacktrace->len - 1 ? "├" : "└",
			src_info->filename.len, src_info->filename.data,
			src_info->range.start.line, src_info->range.start.col, src_info->range.start.byte_offset,
			src_info->range.end.line, src_info->range.end.col, src_info->range.end.byte_offset
		);
	}
}
