#include "internal.h"
#include "nanoprintf.h"

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
mara_print_indented(
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
}

MARA_PRIVATE void
mara_do_print_value(
	mara_exec_ctx_t* ctx,
	mara_value_t value,
	mara_print_options_t options,
	mara_debug_info_key_t debug_key,
	mara_writer_t output
) {
	if (mara_value_is_nil(value)) {
		mara_print_indented(output, options.indent, "nil");
	} else if (mara_value_is_true(value)) {
		mara_print_indented(output, options.indent, "true");
	} else if (mara_value_is_false(value)) {
		mara_print_indented(output, options.indent, "false");
	} else if (mara_value_is_symbol(value) || mara_value_is_str(value)) {
		// TODO: escape new lines and quotes
		mara_str_t result = { 0 };
		mara_value_to_str(ctx, value, &result);
		bool need_quotes = mara_value_is_str(value);
		mara_print_indented(
			output, options.indent,
			"%s%.*s%s",
			need_quotes ? "\"" : "",
			result.len, result.data,
			need_quotes ? "\"" : ""
		);
	} else if (mara_value_is_int(value)) {
		mara_index_t result;
		mara_value_to_int(ctx, value, &result);
		mara_print_indented(output,options.indent, "%d", result);
	} else if (mara_value_is_real(value)) {
		double result;
		mara_value_to_real(ctx, value, &result);
		mara_print_indented(output,options.indent, "%f", result);
	} else if (mara_value_is_list(value)) {
		mara_obj_list_t* list;
		mara_error_t* error = mara_unbox_list(ctx, value, &list);
		(void)error;
		mara_assert(error == NULL, "Could not unbox list");
		debug_key = mara_make_debug_info_key(value, MARA_DEBUG_INFO_SELF);

		if (options.max_depth <= 0) {
			mara_print_indented(
				output, options.indent,
				"(...)  ; %d element%s",
				list->len, list->len > 1 ? "s" : ""
			);
		} else {
			mara_print_indented(output, options.indent, "(\n");
			{
				mara_print_options_t children_options = options;
				children_options.max_depth -= 1;
				children_options.indent += 1;

				mara_index_t print_len = mara_min(list->len, options.max_length);
				for (mara_index_t i = 0; i < print_len; ++i) {
					mara_debug_info_key_t child_key = mara_make_debug_info_key(value, i);
					mara_do_print_value(
						ctx, list->elems[i], children_options, child_key, output
					);
				}

				mara_index_t num_omitted_elments = list->len - print_len;
				if (num_omitted_elments > 0) {
					mara_print_indented(
						output, children_options.indent,
						"... ; %d more element%s",
						num_omitted_elments, num_omitted_elments ? "s" : ""
					);
				}
			}
			mara_print_indented(output, options.indent, ")");
		}
	} else if (mara_value_is_map(value)) {
		mara_obj_map_t* map;
		mara_error_t* error = mara_unbox_map(ctx, value, &map);
		(void)error;
		mara_assert(error == NULL, "Could not unbox map");
		if (options.max_depth <= 0) {
			mara_print_indented(
				output, options.indent,
				"(map ...)  ; %d element%s",
				map->len, map->len > 1 ? "s" : ""
			);
		} else {
			mara_print_indented(output, options.indent, "(map\n");
			{
				mara_print_options_t children_options = options;
				mara_debug_info_key_t children_key = mara_make_debug_info_key(
					mara_nil(), 0
				);
				children_options.max_depth -= 1;
				children_options.indent += 2;

				mara_index_t print_len = mara_min(map->len, options.max_length);
				mara_index_t i = 0;
				for (
					mara_obj_map_node_t* node = map->root;
					i < print_len && node != NULL;
					++i, node = node->next
				) {
					mara_print_indented(output, options.indent + 1, "(\n");
					{
						mara_do_print_value(ctx, node->key, children_options, children_key, output);
						mara_do_print_value(ctx, node->value, children_options, children_key, output);
					}
					mara_print_indented(output, options.indent + 1, ")\n");
				}

				mara_index_t num_omitted_elments = map->len - print_len;
				if (num_omitted_elments > 0) {
					mara_print_indented(
						output, children_options.indent,
						"... ; %d more element%s",
						num_omitted_elments, num_omitted_elments ? "s" : ""
					);
				}
			}
			mara_print_indented(output, options.indent, ")");
		}
	} else if (mara_value_is_function(value)) {
		mara_print_indented(output,options.indent, "(fn ...)");
	} else {
		// TODO: rename ref->handle and handle tag should be meaningful
		mara_print_indented(output,options.indent, "(ref ...)");
	}

	const mara_source_info_t* debug_info = mara_get_debug_info(ctx, debug_key);
	if (debug_info != NULL) {
		mara_fprintf(
			output, " ; @%.*s:%d:%d:%d - %d:%d:%d\n",
			debug_info->filename.len, debug_info->filename.data,
			debug_info->range.start.line,
			debug_info->range.start.col,
			debug_info->range.start.byte_offset,
			debug_info->range.end.line,
			debug_info->range.end.col,
			debug_info->range.end.byte_offset
		);
	} else {
		mara_putc('\n', &output);
	}
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

	mara_do_print_value(ctx, value, options, mara_make_debug_info_key(mara_nil(), 0), output);
}

void
mara_print_error(
	mara_exec_ctx_t* ctx,
	mara_error_t* error,
	mara_print_options_t options,
	mara_writer_t output
) {
	if (error == NULL) { return; }

	mara_fprintf(
		output, "┌[%.*s] %.*s\n",
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

	if (!mara_value_is_nil(error->extra)) {
		mara_fprintf(output, "Extra:\n");
		mara_print_value(ctx, error->extra, options, output);
	}
}
