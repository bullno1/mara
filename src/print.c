#include "internal.h"
#include "nanoprintf.h"
#include "vm.h"

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
	mara_index_t indent,
	const char* fmt,
	...
)  {
	mara_fprintf(output, "%*s", indent * 2, "");

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
);

MARA_PRIVATE void
mara_print_omitted_ellipsis(
	mara_writer_t output,
	mara_index_t indent,
	mara_index_t num_omitted_elments
) {
	if (num_omitted_elments > 0) {
		mara_print_indented(
			output, indent,
			"... ; %d more element%s\n",
			num_omitted_elments, num_omitted_elments ? "s" : ""
		);
	}
}

MARA_PRIVATE void
mara_print_vm_function(
	mara_exec_ctx_t* ctx,
	mara_vm_function_t* fn,
	mara_print_options_t options,
	mara_writer_t output
) {
	(void)ctx;
	if (options.max_depth <= 0) {
		mara_print_indented(output, options.indent, "(code %p)\n", (void*)fn);
	} else {
		mara_print_indented(output, options.indent, "(code\n");
		mara_print_options_t body_options = options;
		body_options.indent += 2;
		body_options.max_depth -= 1;

		mara_print_indented(output, options.indent + 1, "(info\n");
		{
			mara_print_indented(output, body_options.indent, "num-args %d\n", fn->num_args);
			mara_print_indented(output, body_options.indent, "num-locals %d\n", fn->num_locals);
			mara_print_indented(output, body_options.indent, "num-captures %d\n", fn->num_captures);
			mara_print_indented(output, body_options.indent, "stack-size %d\n", fn->stack_size);
		}
		mara_print_indented(output, options.indent + 1, ")\n");

		mara_print_indented(output, options.indent + 1, "(instructions\n");
		{
			mara_index_t num_instructions = fn->num_instructions;
			mara_index_t print_len = mara_min(num_instructions, options.max_length);
			for (mara_index_t i = 0; i < print_len; ++i) {
				mara_instruction_t instruction = fn->instructions[i];
				mara_opcode_t opcode;
				mara_operand_t operands;
				mara_decode_instruction(instruction, &opcode, &operands);

				switch (opcode) {
					case MARA_OP_NOP:
						mara_print_indented(output, body_options.indent, "(NOP)");
						break;
					case MARA_OP_NIL:
						mara_print_indented(output, body_options.indent, "(NIL)");
						break;
					case MARA_OP_TRUE:
						mara_print_indented(output, body_options.indent, "(TRUE)");
						break;
					case MARA_OP_FALSE:
						mara_print_indented(output, body_options.indent, "(FALSE)");
						break;
					case MARA_OP_SMALL_INT:
						mara_print_indented(output, body_options.indent, "(SMALL_INT %d)", (int16_t)operands);
						break;
					case MARA_OP_CONSTANT:
						mara_print_indented(output, body_options.indent, "(CONSTANT %d)", operands);
						break;
					case MARA_OP_POP:
						mara_print_indented(output, body_options.indent, "(POP %d)", (int16_t)operands);
						break;
					case MARA_OP_SET_LOCAL:
						mara_print_indented(output, body_options.indent, "(SET_LOCAL %d)", operands);
						break;
					case MARA_OP_GET_LOCAL:
						mara_print_indented(output, body_options.indent, "(GET_LOCAL %d)", operands);
						break;
					case MARA_OP_SET_ARG:
						mara_print_indented(output, body_options.indent, "(SET_ARG %d)", operands);
						break;
					case MARA_OP_GET_ARG:
						mara_print_indented(output, body_options.indent, "(GET_ARG %d)", operands);
						break;
					case MARA_OP_SET_CAPTURE:
						mara_print_indented(output, body_options.indent, "(SET_CAPTURE %d)", operands);
						break;
					case MARA_OP_GET_CAPTURE:
						mara_print_indented(output, body_options.indent, "(GET_CAPTURE %d)", operands);
						break;
					case MARA_OP_CALL:
						mara_print_indented(output, body_options.indent, "(CALL %d)", operands);
						break;
					case MARA_OP_RETURN:
						mara_print_indented(output, body_options.indent, "(RETURN)");
						break;
					case MARA_OP_JUMP:
						mara_print_indented(output, body_options.indent, "(JUMP %d)", (int16_t)operands);
						break;
					case MARA_OP_JUMP_IF_FALSE:
						mara_print_indented(output, body_options.indent, "(JUMP_IF_FALSE %d)", (int16_t)operands);
						break;
					case MARA_OP_MAKE_CLOSURE:
						mara_print_indented(output, body_options.indent, "(MAKE_CLOSURE %d %d)",
							(uint8_t)(operands >> 16) & 0xff,
							operands & 0xffff
						);
						break;
					case MARA_OP_CALL_CAPTURE:
						mara_print_indented(output, body_options.indent, "(CALL_CAPTURE %d %d)",
							(uint8_t)(operands >> 16) & 0xff,
							operands & 0xffff
						);
						break;
					case MARA_OP_CALL_ARG:
						mara_print_indented(output, body_options.indent, "(CALL_ARG %d %d)",
							(uint8_t)(operands >> 16) & 0xff,
							operands & 0xffff
						);
						break;
					case MARA_OP_CALL_LOCAL:
						mara_print_indented(output, body_options.indent, "(CALL_LOCAL %d %d)",
							(uint8_t)(operands >> 16) & 0xff,
							operands & 0xffff
						);
						break;
					case MARA_OP_LT:
						mara_print_indented(output, body_options.indent, "(LT)");
						break;
					case MARA_OP_LTE:
						mara_print_indented(output, body_options.indent, "(LTE)");
						break;
					case MARA_OP_GT:
						mara_print_indented(output, body_options.indent, "(GT)");
						break;
					case MARA_OP_GTE:
						mara_print_indented(output, body_options.indent, "(GTE)");
						break;
					case MARA_OP_PLUS:
						mara_print_indented(output, body_options.indent, "(PLUS %d)", operands);
						break;
					case MARA_OP_SUB:
						mara_print_indented(output, body_options.indent, "(SUB %d)", operands);
						break;
					case MARA_OP_MAKE_LIST:
						mara_print_indented(output, body_options.indent, "(MAKE_LIST %d)", operands);
						break;
					case MARA_OP_PUT:
						mara_print_indented(output, body_options.indent, "(PUT)");
						break;
					case MARA_OP_GET:
						mara_print_indented(output, body_options.indent, "(GET)");
						break;
					case MARA_OP_NEG:
						mara_print_indented(output, body_options.indent, "(NEG)");
						break;
				}

				if (fn->source_info != NULL) {
					mara_source_info_t* debug_info = &fn->source_info[i];
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
				}
			}
			mara_print_omitted_ellipsis(output, body_options.indent, num_instructions - print_len);
		}
		mara_print_indented(output, options.indent + 1, ")\n");

		mara_print_indented(output, options.indent + 1, "(constants\n");
		{
			mara_index_t num_constants = fn->num_constants;
			mara_index_t print_len = mara_min(num_constants, options.max_length);
			for (mara_index_t i = 0; i < print_len; ++i) {
				mara_do_print_value(
					ctx,
					fn->constants[i],
					body_options,
					mara_make_debug_info_key(mara_nil(), 0),
					output
				);
			}
			mara_print_omitted_ellipsis(output, body_options.indent, num_constants - print_len);
		}
		mara_print_indented(output, options.indent + 1, ")\n");

		mara_print_indented(output, options.indent + 1, "(functions\n");
		{
			mara_index_t num_functions = fn->num_functions;
			mara_index_t print_len = mara_min(num_functions, options.max_length);
			for (mara_index_t i = 0; i < print_len; ++i) {
				mara_print_vm_function(ctx, fn->functions[i], body_options, output);
			}
			mara_print_omitted_ellipsis(output, body_options.indent, num_functions - print_len);
		}
		mara_print_indented(output, options.indent + 1, ")\n");

		mara_print_indented(output, options.indent, ")\n");
	}
}

MARA_PRIVATE void
mara_do_print_value(
	mara_exec_ctx_t* ctx,
	mara_value_t value,
	mara_print_options_t options,
	mara_debug_info_key_t debug_key,
	mara_writer_t output
) {
	mara_debug_info_key_t dummy_key = mara_make_debug_info_key(mara_nil(), 0);

	if (mara_value_is_nil(value)) {
		mara_print_indented(output, options.indent, "nil");
	} else if (mara_value_is_true(value)) {
		mara_print_indented(output, options.indent, "true");
	} else if (mara_value_is_false(value)) {
		mara_print_indented(output, options.indent, "false");
	} else if (mara_value_is_sym(value) || mara_value_is_str(value)) {
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
		mara_real_t result;
		mara_value_to_real(ctx, value, &result);
		mara_print_indented(output,options.indent, "%f", result);
	} else if (mara_value_is_list(value)) {
		mara_list_t* list;
		mara_assert_no_error(mara_value_to_list(ctx, value, &list));
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
				mara_print_omitted_ellipsis(output, children_options.indent, list->len - print_len);
			}
			mara_print_indented(output, options.indent, ")");
		}
	} else if (mara_value_is_map(value)) {
		mara_map_t* map;
		mara_assert_no_error(mara_value_to_map(ctx, value, &map));
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
				children_options.max_depth -= 1;
				children_options.indent += 2;

				mara_index_t print_len = mara_min(map->len, options.max_length);
				mara_index_t i = 0;
				for (
					mara_map_node_t* node = map->root;
					i < print_len && node != NULL;
					++i, node = node->next
				) {
					mara_print_indented(output, options.indent + 1, "(\n");
					{
						mara_do_print_value(ctx, node->key, children_options, dummy_key, output);
						mara_do_print_value(ctx, node->value, children_options, dummy_key, output);
					}
					mara_print_indented(output, options.indent + 1, ")\n");
				}
				mara_print_omitted_ellipsis(output, options.indent, map->len - print_len);
			}
			mara_print_indented(output, options.indent, ")");
		}
	} else if (mara_value_is_fn(value)) {
		mara_obj_t* obj = mara_value_to_obj(value);
		if (obj->type == MARA_OBJ_TYPE_NATIVE_CLOSURE) {
			mara_native_closure_t* closure = (mara_native_closure_t*)obj->body;
			mara_print_indented(output, options.indent, "(native-fn\n");
			{
				mara_print_options_t children_options = options;
				children_options.max_depth -= 1;
				children_options.indent += 1;

				mara_print_indented(output, options.indent + 1, "%p\n", (void*)closure->fn);
				mara_print_indented(output, options.indent + 1, "%s\n", closure->no_alloc ? "no-alloc" : "alloc");
				mara_do_print_value(ctx, closure->userdata, children_options, dummy_key, output);
			}
			mara_print_indented(output, options.indent, ")");
		} else {
			mara_vm_closure_t* closure = (mara_vm_closure_t*)obj->body;
			if (options.max_depth <= 0) {
				mara_print_indented(
					output, options.indent,
					"(fn (closure %p %p))",
					(void*)closure, (void*)closure->fn
				);
			} else {
				mara_print_indented(output, options.indent, "(fn\n");
				{
					mara_print_indented(output, options.indent + 1, "(captures\n");
					{
						mara_print_options_t capture_print_options = options;
						capture_print_options.indent += 2;
						capture_print_options.max_depth -= 1;

						mara_index_t num_captures = closure->fn->num_captures;
						mara_index_t print_len = mara_min(num_captures, options.max_length);
						for (mara_index_t i = 0; i < print_len; ++i) {
							mara_do_print_value(
								ctx, closure->captures[i], capture_print_options,
								dummy_key,
								output
							);
						}
						mara_print_omitted_ellipsis(
							output,
							capture_print_options.indent,
							num_captures - print_len
						);
					}
					mara_print_indented(output, options.indent + 1, ")\n");

					{
						mara_print_options_t code_print_options = options;
						code_print_options.indent += 1;
						code_print_options.max_depth -= 1;

						mara_print_vm_function(ctx, closure->fn, code_print_options, output);
					}
				}
				mara_print_indented(output, options.indent, ")");
			}
		}
	} else {
		// TODO: rename ref->handle and handle tag should be meaningful
		mara_print_indented(output, options.indent, "(ref ...)");
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
	mara_set_default_option(&options.max_length, 100);
	mara_set_default_option(&options.max_depth, 7);

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
		const char* first_connector = " ";
		const char* second_connector = "├";
		if (i == 0) {
			first_connector = "└";
			if (i < error->stacktrace->len - 1) {
				second_connector = "┬";
			} else {
				second_connector = "─";
			}
		} else if (i == error->stacktrace->len - 1) {
			second_connector = "└";
		}

		mara_fprintf(
			output, "%s%s%.*s:%d:%d:%d - %d:%d:%d\n",
			first_connector, second_connector,
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
