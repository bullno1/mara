#include "internal.h"
#include "mara.h"
#include "nanoprintf.h"
#include <stdarg.h>

MARA_PRIVATE void
mara_putc(int ch, void* ctx) {
	mara_writer_t* writer = ctx;
	mara_write(&ch, 1, *writer);
}

MARA_PRIVATE void
mara_set_default_option(mara_index_t* input, mara_index_t default_value){
	if (*input == 0) {
		*input = default_value;
	} else if (*input < 0) {
		*input = INT32_MAX;
	}
}

MARA_PRIVATE void
mara_vfprintf(mara_writer_t output, const char* fmt, va_list args) {
	npf_vpprintf(mara_putc, &output, fmt, args);
}

MARA_PRIVATE MARA_PRINTF_LIKE(2, 3) void
mara_fprintf(mara_writer_t output, const char* fmt, ...)  {
	va_list args;
	va_start(args, fmt);
	mara_vfprintf(output, fmt, args);
	va_end(args);
}

MARA_PRIVATE MARA_PRINTF_LIKE(3, 4) void
mara_printline_indented(
	mara_writer_t output,
	int indent,
	const char* fmt,
	...
)  {
	mara_fprintf(output, "%*s", indent, "");

	va_list args;
	va_start(args, fmt);
	mara_vfprintf(output, fmt, args);
	va_end(args);

	mara_putc('\n', &output);
}

MARA_PRIVATE void
mara_do_print_value(
	mara_exec_ctx_t* ctx,
	mara_value_t value,
	mara_print_options_t options,
	mara_writer_t output
) {
	if (mara_value_is_nil(value)) {
		mara_printline_indented(output, options.indent, "nil");
	} else if (mara_value_is_true(value)) {
		mara_printline_indented(output, options.indent, "true");
	} else if (mara_value_is_false(value)) {
		mara_printline_indented(output, options.indent, "false");
	} else if (mara_value_is_symbol(value) || mara_value_is_str(value)) {
		// TODO: escape new lines and quotes
		mara_str_t result = { 0 };
		mara_value_to_str(ctx, value, &result);
		bool need_quotes = mara_value_is_str(value);
		mara_printline_indented(
			output, options.indent,
			"%s%.*s%s",
			need_quotes ? "\"" : "",
			result.len, result.data,
			need_quotes ? "\"" : ""
		);
	} else if (mara_value_is_int(value)) {
		mara_index_t result;
		mara_value_to_int(ctx, value, &result);
		mara_printline_indented(output,options.indent, "%d", result);
	} else if (mara_value_is_real(value)) {
		double result;
		mara_value_to_real(ctx, value, &result);
		mara_printline_indented(output,options.indent, "%f", result);
	} else if (mara_value_is_list(value)) {
		mara_obj_list_t* list;
		mara_assert(mara_unbox_list(ctx, value, &list) == NULL, "Could not unbox list");
		if (options.max_depth <= 0) {
			mara_printline_indented(
				output, options.indent,
				"(...)  ; %d element%s",
				list->len, list->len > 1 ? "s" : ""
			);
		} else {
			mara_printline_indented(output, options.indent, "(");
			{
				mara_print_options_t children_options = options;
				children_options.max_depth -= 1;
				children_options.indent += 1;

				mara_index_t print_len = mara_min(list->len, options.max_length);
				for (mara_index_t i = 0; i < print_len; ++i) {
					mara_do_print_value(ctx, list->elems[i], children_options, output);
				}

				mara_index_t num_omitted_elments = list->len - print_len;
				if (num_omitted_elments > 0) {
					mara_printline_indented(
						output, children_options.indent,
						"... ; %d more element%s",
						num_omitted_elments, num_omitted_elments ? "s" : ""
					);
				}
			}
			mara_printline_indented(output, options.indent, ")");
		}
	} else if (mara_value_is_map(value)) {
		mara_obj_map_t* map;
		mara_assert(mara_unbox_map(ctx, value, &map) == NULL, "Could not unbox map");
		if (options.max_depth <= 0) {
			mara_printline_indented(
				output, options.indent,
				"(map ...)  ; %d element%s",
				map->len, map->len > 1 ? "s" : ""
			);
		} else {
			mara_printline_indented(output, options.indent, "(map");
			{
				mara_print_options_t children_options = options;
				children_options.max_depth -= 1;
				children_options.indent += 2;

				mara_index_t print_len = mara_min(map->len, options.max_length);
				mara_index_t i = 0;
				for (
					mara_obj_map_node_t* node = map->root;
					i < print_len && node != NULL;
					++i, node = node->next
				) {
					mara_printline_indented(output, options.indent + 1, "(");
					{
						mara_do_print_value(ctx, node->key, children_options, output);
						mara_do_print_value(ctx, node->value, children_options, output);
					}
					mara_printline_indented(output, options.indent + 1, ")");
				}

				mara_index_t num_omitted_elments = map->len - print_len;
				if (num_omitted_elments > 0) {
					mara_printline_indented(
						output, children_options.indent,
						"... ; %d more element%s",
						num_omitted_elments, num_omitted_elments ? "s" : ""
					);
				}
			}
			mara_printline_indented(output, options.indent, ")");
		}
	} else if (mara_value_is_function(value)) {
		mara_printline_indented(output,options.indent, "(fn ...)");
	} else {
		// TODO: rename ref->handle and handle tag should be meaningful
		mara_printline_indented(output,options.indent, "(ref ...)");
	}
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
) {
	mara_set_default_option(&options.max_length, 10);
	mara_set_default_option(&options.max_depth, 4);
	mara_do_print_value(ctx, value, options, output);
}

void
mara_print_error(
	mara_exec_ctx_t* ctx,
	mara_error_t* error,
	mara_print_options_t options,
	mara_writer_t output
) {
	(void)ctx;
	(void)options;

	mara_fprintf(
		output, "┌error:%.*s: %.*s\n",
		error->type.len, error->type.data,
		error->message.len, error->message.data
	);
	for (
		mara_index_t i = 0;
		error->stacktrace != NULL && i < error->stacktrace->len;
		++i
	) {
		mara_source_info_t* src_info = &error->stacktrace->frames[i];
		mara_fprintf(
			output, "%s─%.*s:%d:%d:%d - %d:%d:%d\n",
			i < error->stacktrace->len - 1 ? "├" : "└",
			src_info->filename.len, src_info->filename.data,
			src_info->range.start.line, src_info->range.start.col, src_info->range.start.byte_offset,
			src_info->range.end.line, src_info->range.end.col, src_info->range.end.byte_offset
		);
	}
}
