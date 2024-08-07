#include "internal.h"
#include "lexer.h"
#include <stdlib.h>
#include <errno.h>

typedef struct mara_list_link_s {
	struct mara_list_link_s* next;
	struct mara_list_link_s* prev;
} mara_list_link_t;

typedef struct {
	mara_list_link_t link;
	mara_value_t value;
	mara_source_range_t source_range;
} mara_list_node_t;

typedef struct {
	mara_index_t len;
	mara_list_link_t link;
	mara_source_range_t source_range;
} mara_linked_list_t;

MARA_PRIVATE mara_error_t*
mara_parser_error(
	mara_exec_ctx_t* ctx,
	mara_lexer_t* lexer,
	mara_str_t type,
	const char* fmt,
	mara_source_range_t range,
	...
) {
	// This is safe, we immediately call mara_errorv which copies this debug_info
	mara_source_info_t debug_info = {
		.filename = lexer->filename,
		.range = range,
	};
	mara_set_debug_info(ctx, &debug_info);

	va_list args;
	va_start(args, range);
	mara_error_t* result = mara_errorv(ctx, type, fmt, mara_nil(), args);
	va_end(args);
	return result;
}

MARA_PRIVATE void
mara_linked_list_init(mara_linked_list_t* list, mara_source_pos_t start) {
	list->len = 0;
	list->link.next = &list->link;
	list->link.prev = &list->link;
	list->source_range.start = start;
}

MARA_PRIVATE void
mara_linked_list_push(
	mara_exec_ctx_t* ctx,
	mara_linked_list_t* list,
	mara_value_t value,
	mara_source_range_t source_range
) {
	mara_list_node_t* node = MARA_ZONE_ALLOC_TYPE(
		ctx, mara_get_local_zone(ctx), mara_list_node_t
	);
	node->value = value;
	node->source_range = source_range;
	node->link.next = &list->link;
	node->link.prev = list->link.prev;
	list->link.prev->next = &node->link;
	list->link.prev = &node->link;
	list->len += 1;
}

MARA_PRIVATE mara_list_t*
mara_linked_list_flatten(
	mara_exec_ctx_t* ctx,
	mara_zone_t* zone,
	mara_str_t filename,
	mara_linked_list_t* tmp_list
) {
	mara_list_t* list = mara_new_list(ctx, zone, tmp_list->len);
	mara_put_debug_info(
		ctx,
		mara_make_debug_info_key(mara_value_from_list(list), MARA_DEBUG_INFO_SELF),
		(mara_source_info_t){
			.filename = filename,
			.range = tmp_list->source_range
		}
	);
	mara_index_t list_index = 0;
	for (
		mara_list_link_t* itr = tmp_list->link.next;
		itr != &tmp_list->link;
		itr = itr->next, ++list_index
	) {
		mara_list_node_t* node = mara_container_of(itr, mara_list_node_t, link);
		mara_list_push(ctx, list, node->value);
		// TODO: this can be sped up
		// We push the same filename repeatedly
		mara_put_debug_info(
			ctx,
			mara_make_debug_info_key(mara_value_from_list(list), list_index),
			(mara_source_info_t){
			.filename = filename,
				.range = node->source_range,
			}
		);
	}
	return list;
}

MARA_PRIVATE mara_error_t*
mara_parse_token(
	mara_exec_ctx_t* ctx,
	mara_zone_t* zone,
	mara_lexer_t* lexer,
	mara_token_t token,
	mara_value_t* result
);

MARA_PRIVATE mara_error_t*
mara_parse_list(
	mara_exec_ctx_t* ctx,
	mara_zone_t* zone,
	mara_lexer_t* lexer,
	mara_linked_list_t* list
) {
	while (true) {
		mara_token_t token;
		mara_check_error(mara_lexer_next(ctx, lexer, &token));

		switch (token.type) {
			case MARA_TOK_END:
				return mara_parser_error(
					ctx,
					lexer,
					mara_str_from_literal("core/syntax/bad-list"),
					"Badly formatted list",
					(mara_source_range_t){
						.start = list->source_range.start,
						.end = token.location.end,
					}
				);
			case MARA_TOK_INT:
			case MARA_TOK_REAL:
			case MARA_TOK_STRING:
			case MARA_TOK_SYMBOL:
			case MARA_TOK_LEFT_PAREN:
				{
					mara_value_t elem;
					mara_error_t* error = mara_parse_token(ctx, zone, lexer, token, &elem);
					if (error != NULL) { return error; }

					mara_linked_list_push(ctx, list, elem, token.location);
				}
				break;
			case MARA_TOK_RIGHT_PAREN:
				list->source_range.end = token.location.end;
				return NULL;
			default:
				mara_add_native_debug_info(ctx);
				return mara_errorf(
					ctx,
					mara_str_from_literal("core/panic"),
					"Invalid state",
					mara_nil()
				);
		}
	}
}

MARA_PRIVATE mara_error_t*
mara_parse_token(
	mara_exec_ctx_t* ctx,
	mara_zone_t* zone,
	mara_lexer_t* lexer,
	mara_token_t token,
	mara_value_t* result
) {
	mara_arena_t* local_arena = &mara_get_local_zone(ctx)->arena;

	switch (token.type) {
		case MARA_TOK_INT:
			{
				mara_index_t value;
				mara_index_t error;
				mara_arena_snapshot_t snapshot = mara_arena_snapshot(ctx->env, local_arena);
				{
					char* str = mara_arena_alloc_ex(
						ctx->env, local_arena,
						token.lexeme.len + 1, _Alignof(char)
					);
					mara_index_t len = 0;
					for (mara_index_t i = 0; i < token.lexeme.len; ++i) {
						if (token.lexeme.data[i] != '_') {
							str[len] = token.lexeme.data[i];
							++len;
						}
					}
					str[len] = '\0';
					errno = 0;
					value = strtol(str, NULL, 10);
					error = errno;
				}
				mara_arena_restore(ctx->env, local_arena, snapshot);

				if (error == 0) {
					*result = mara_value_from_int(value);
					return NULL;
				} else {
					return mara_parser_error(
						ctx,
						lexer,
						mara_str_from_literal("core/syntax/bad-number"),
						"Number too large",
						token.location
					);
				}
			}
		case MARA_TOK_REAL:
			{
				mara_real_t value;
				mara_index_t error;
				mara_arena_snapshot_t snapshot = mara_arena_snapshot(ctx->env, local_arena);
				{
					char* str = mara_arena_alloc_ex(
						ctx->env, local_arena,
						token.lexeme.len + 1, _Alignof(char)
					);
					mara_index_t len = 0;
					for (mara_index_t i = 0; i < token.lexeme.len; ++i) {
						if (token.lexeme.data[i] != '_') {
							str[len] = token.lexeme.data[i];
							++len;
						}
					}
					str[len] = '\0';
					errno = 0;
					value = strtod(str, NULL);
					error = errno;
				}
				mara_arena_restore(ctx->env, local_arena, snapshot);

				if (error == 0) {
					*result = mara_value_from_real(value);
					return NULL;
				} else {
					return mara_parser_error(
						ctx,
						lexer,
						mara_str_from_literal("core/syntax/bad-number"),
						"Number too large",
						token.location
					);
				}
			}
		case MARA_TOK_STRING:
			{
				mara_arena_snapshot_t snapshot = mara_arena_snapshot(ctx->env, local_arena);
				mara_error_t* error = NULL;
				char* str = mara_arena_alloc_ex(
					ctx->env, local_arena, token.lexeme.len, _Alignof(char)
				);
				mara_index_t len = 0;
				for (mara_index_t i = 0; i < token.lexeme.len; ++i) {
					char ch = token.lexeme.data[i];
					if (ch == '\\') {
						if (++i < token.lexeme.len) {
							ch = token.lexeme.data[i];
							switch (ch) {
								case 'n':
									str[len++] = '\n';
									break;
								case 'r':
									str[len++] = '\r';
									break;
								case 't':
									str[len++] = '\t';
									break;
								default:
									str[len++] = ch;
									break;
							}
						}
					} else {
						str[len++] = ch;
					}
				}
				if (error == NULL) {
					*result = mara_new_str(ctx, zone, (mara_str_t){
						.len = len,
						.data = str,
					});
				}
				mara_arena_restore(ctx->env, local_arena, snapshot);
				return error;
			}
		case MARA_TOK_SYMBOL:
			*result = mara_new_sym(ctx, token.lexeme);
			return NULL;
		case MARA_TOK_RIGHT_PAREN:
			return mara_parser_error(
				ctx,
				lexer,
				mara_str_from_literal("core/syntax/unexpected-token"),
				"Unexpected ')'",
				token.location
			);
		case MARA_TOK_LEFT_PAREN:
			{
				mara_error_t* error = NULL;
				mara_arena_snapshot_t snapshot = mara_arena_snapshot(ctx->env, local_arena);
				{
					mara_linked_list_t tmp_list;
					mara_linked_list_init(&tmp_list, token.location.start);
					error = mara_parse_list(ctx, zone, lexer, &tmp_list);
					if (error == NULL) {
						mara_list_t* list = mara_linked_list_flatten(
							ctx, zone, lexer->filename, &tmp_list
						);
						*result = mara_value_from_list(list);
					}
				}
				mara_arena_restore(ctx->env, local_arena, snapshot);
				return error;
			}
		case MARA_TOK_END:
			return mara_parser_error(
				ctx,
				lexer,
				mara_str_from_literal("core/syntax/unexpected-eof"),
				"Unexpected end of file",
				token.location
			);
		default:
			mara_add_native_debug_info(ctx);
			return mara_errorf(
				ctx,
				mara_str_from_literal("core/panic"),
				"Invalid state",
				mara_nil()
			);
	}
}

MARA_PRIVATE mara_error_t*
mara_do_parse(
	mara_exec_ctx_t* ctx,
	mara_zone_t* zone,
	mara_parse_options_t options,
	mara_reader_t reader,
	mara_list_t** result
) {
	mara_str_t filename = options.filename;
	if (filename.len == 0) {
		filename = mara_str_from_literal("<unknown>");
	}

	mara_lexer_t lexer;
	mara_lexer_init(&lexer, filename, reader);

	mara_error_t* error = NULL;
	mara_linked_list_t tmp_list;
	mara_linked_list_init(&tmp_list, (mara_source_pos_t){
		.byte_offset = 0,
		.line = 1,
		.col = 1,
	});

	while (true) {
		mara_token_t token;
		error = mara_lexer_next(ctx, &lexer, &token);
		if (error != NULL) { break; }
		if (token.type == MARA_TOK_END) { break; }

		mara_value_t elem;
		error = mara_parse_token(ctx, zone, &lexer, token, &elem);
		if (error != NULL) { break; }

		mara_linked_list_push(ctx, &tmp_list, elem, token.location);

		if (options.parse_one) { break; }
	}

	if (error == NULL) {
		*result = mara_linked_list_flatten(ctx, zone, filename, &tmp_list);
	}

	return error;
}

mara_error_t*
mara_parse(
	mara_exec_ctx_t* ctx,
	mara_zone_t* zone,
	mara_parse_options_t options,
	mara_reader_t reader,
	mara_list_t** result
) {
	mara_error_t* error;
	mara_zone_t* parser_zone = mara_zone_enter(ctx);
	if (parser_zone == NULL) {
		return mara_errorf(
			ctx,
			mara_str_from_literal("core/limit-reached/stack-overflow"),
			"Too many stack frames",
			mara_nil()
		);
	}

	error = mara_do_parse(ctx, zone, options, reader, result);
	mara_zone_exit(ctx, parser_zone);
	return error;
}
