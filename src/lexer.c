#include "lexer.h"
#include <mara/utils.h>

MARA_PRIVATE mara_error_t*
mara_lexer_error(
	mara_exec_ctx_t* ctx,
	mara_lexer_t* lexer,
	mara_str_t type,
	const char* fmt,
	mara_value_t extra,
	...
) {
	mara_set_debug_info(ctx, (mara_source_info_t){
		.filename = lexer->filename,
		.range = {
			.start = lexer->capturing ? lexer->capture_start_pos : lexer->current_pos,
			.end = lexer->current_pos,
		},
	});

	va_list args;
	va_start(args, extra);
	mara_error_t* result = mara_errorv(ctx, type, fmt, extra, args);
	va_end(args);
	return result;
}

MARA_PRIVATE mara_error_t*
mara_lexer_peek(mara_exec_ctx_t* ctx, mara_lexer_t* lexer, char* result) {
	if (lexer->buffered) {
		*result = lexer->read_buf;
		return NULL;
	} else {
		mara_index_t bytes_read = mara_read(&lexer->read_buf, 1, lexer->src);
		if (bytes_read == 1) {
			lexer->buffered = true;
			*result = lexer->read_buf;
			return NULL;
		} else if (bytes_read == 0) {
			lexer->buffered = true;
			lexer->read_buf = *result = 0;
			return NULL;
		} else {
			return mara_lexer_error(
				ctx,
				lexer,
				mara_str_from_literal("core/io-error"),
				"Reader returned: %d",
				mara_value_from_int(bytes_read),
				bytes_read
			);
		}
	}
}

MARA_PRIVATE mara_error_t*
mara_lexer_consume_char(mara_exec_ctx_t* ctx, mara_lexer_t* lexer) {
	if (!lexer->buffered) { return NULL; }

	char read_buf = lexer->read_buf;
	lexer->buffered = false;
	lexer->current_pos.col += 1;
	lexer->current_pos.byte_offset += 1;

	if (lexer->capturing) {
		if (lexer->capture_len >= sizeof(lexer->capture_buf)) {
			return mara_lexer_error(
				ctx,
				lexer,
				mara_str_from_literal("core/syntax/element-too-long"),
				"Element is too long",
				mara_null()
			);
		}

		lexer->capture_buf[lexer->capture_len++] = read_buf;
	}

	return NULL;
}

MARA_PRIVATE bool
mara_lexer_is_space(char ch) {
	return ch == ' '
		|| ch == '\t'
		|| ch == '\v'
		|| ch == '\f';
}

MARA_PRIVATE bool
mara_lexer_is_new_line(char ch) {
	return ch == '\r' || ch == '\n';
}

MARA_PRIVATE bool
mara_lexer_is_end(char ch) {
	return ch == 0;
}

MARA_PRIVATE bool
mara_lexer_is_comment(char ch) {
	return ch == ';';
}

MARA_PRIVATE bool
mara_lexer_is_paren(char ch) {
	return ch == '(' || ch == ')';
}

MARA_PRIVATE bool
mara_lexer_is_digit(char ch) {
	return '0' <= ch && ch <= '9';
}

MARA_PRIVATE mara_error_t*
mara_lexer_handle_comment(mara_exec_ctx_t* ctx, mara_lexer_t* lexer) {
	char ch;

	do {
		mara_check_error(mara_lexer_consume_char(ctx, lexer));
		mara_check_error(mara_lexer_peek(ctx, lexer, &ch));
	} while (!(mara_lexer_is_new_line(ch) || mara_lexer_is_end(ch)));

	return NULL;
}

MARA_PRIVATE mara_error_t*
mara_lexer_handle_new_line(mara_exec_ctx_t* ctx, mara_lexer_t* lexer, char current_char) {
	char next_char;
	mara_check_error(mara_lexer_consume_char(ctx, lexer));
	mara_check_error(mara_lexer_peek(ctx, lexer, &next_char));
	if (current_char == '\r' && next_char == '\n') {
		mara_check_error(mara_lexer_consume_char(ctx, lexer));
	}
	lexer->current_pos.col = 1;
	lexer->current_pos.line += 1;

	return NULL;
}

MARA_PRIVATE mara_token_t
mara_lexer_make_token(mara_lexer_t* lexer, mara_token_type_t type) {
	bool capturing = lexer->capturing;
	return (mara_token_t) {
		.type = type,
		.lexeme = {
			.len = lexer->capture_len,
			.data = capturing ? lexer->capture_buf : NULL,
		},
		.location = {
			.start = capturing ? lexer->capture_start_pos : lexer->current_pos,
			.end = lexer->current_pos,
		},
	};
}

MARA_PRIVATE void
mara_lexer_begin_capture(mara_lexer_t* lexer) {
	lexer->capturing = true;
	lexer->capture_len = 0;
	lexer->capture_start_pos = lexer->current_pos;
}

MARA_PRIVATE mara_error_t*
mara_lexer_continue_number(mara_exec_ctx_t* ctx, mara_lexer_t* lexer, mara_token_t* result) {
	mara_check_error(mara_lexer_consume_char(ctx, lexer));

	bool dotted = false;
	while (true) {
		char ch;
		mara_check_error(mara_lexer_peek(ctx, lexer, &ch));

		if (ch == '.') {
			mara_check_error(mara_lexer_consume_char(ctx, lexer));

			if (dotted) {
				return mara_lexer_error(
					ctx,
					lexer,
					mara_str_from_literal("core/syntax/bad-number"),
					"Badly formatted number",
					mara_null()
				);
			} else {
				dotted = true;
			}
		} else if (ch == '_' || mara_lexer_is_digit(ch)) {
			mara_check_error(mara_lexer_consume_char(ctx, lexer));
		} else if (
			mara_lexer_is_space(ch)
			|| mara_lexer_is_new_line(ch)
			|| mara_lexer_is_end(ch)
			|| mara_lexer_is_comment(ch)
		) {
			*result = mara_lexer_make_token(lexer, dotted ? MARA_TOK_REAL: MARA_TOK_INT);
			return NULL;
		} else {
			return mara_lexer_error(
				ctx,
				lexer,
				mara_str_from_literal("core/syntax/bad-number"),
				"Badly formatted number",
				mara_null()
			);
		}
	}
}

MARA_PRIVATE mara_error_t*
mara_lexer_handle_string(mara_exec_ctx_t* ctx, mara_lexer_t* lexer, mara_token_t* result) {
	char ch;
	mara_check_error(mara_lexer_consume_char(ctx, lexer)); // Skip the quote
	mara_lexer_begin_capture(lexer);
	while (true) {
		mara_check_error(mara_lexer_peek(ctx, lexer, &ch));

		if (ch == '"') {
			*result = mara_lexer_make_token(lexer, MARA_TOK_STRING);
			mara_check_error(mara_lexer_consume_char(ctx, lexer));
			return NULL;
		} else if (ch == '\\') {
			// Escape next char
			mara_check_error(mara_lexer_consume_char(ctx, lexer));
			mara_check_error(mara_lexer_peek(ctx, lexer, &ch));
			if (!(mara_lexer_is_end(ch) || mara_lexer_is_new_line(ch))) {
				mara_check_error(mara_lexer_consume_char(ctx, lexer));
			}
		} else if (mara_lexer_is_end(ch) || mara_lexer_is_new_line(ch)) {
			return mara_lexer_error(
				ctx,
				lexer,
				mara_str_from_literal("core/syntax/bad-string"),
				"Badly formatted string",
				mara_null()
			);
		} else {
			mara_check_error(mara_lexer_consume_char(ctx, lexer));
		}
	}
}

MARA_PRIVATE mara_error_t*
mara_lexer_continue_symbol(mara_exec_ctx_t* ctx, mara_lexer_t* lexer, mara_token_t* result) {
	while (true) {
		char ch;
		mara_check_error(mara_lexer_peek(ctx, lexer, &ch));
		if (
			mara_lexer_is_space(ch)
			|| mara_lexer_is_new_line(ch)
			|| mara_lexer_is_end(ch)
			|| mara_lexer_is_paren(ch)
		) {
			*result = mara_lexer_make_token(lexer, MARA_TOK_SYMBOL);
			return NULL;
		} else {
			mara_check_error(mara_lexer_consume_char(ctx, lexer));
		}
	}
}

void
mara_lexer_init(mara_lexer_t* lexer, mara_str_t filename, mara_reader_t src) {
	*lexer = (mara_lexer_t) {
		.filename = filename,
		.src = src,
		.current_pos = {
			.line = 1,
			.col = 1,
		},
	};
}

mara_error_t*
mara_lexer_next(mara_exec_ctx_t* ctx, mara_lexer_t* lexer, mara_token_t* result) {
	lexer->capturing = false;
	lexer->capture_len = 0;

	while (true) {
		char ch;
		mara_check_error(mara_lexer_peek(ctx, lexer, &ch));

		if (mara_lexer_is_end(ch)) {
			*result = mara_lexer_make_token(lexer, MARA_TOK_END);
			return NULL;
		} else if (mara_lexer_is_space(ch)) {
			mara_check_error(mara_lexer_consume_char(ctx, lexer));
			continue;
		} else if (mara_lexer_is_comment(ch)) {
			mara_check_error(mara_lexer_handle_comment(ctx, lexer));
			continue;
		} else if (mara_lexer_is_new_line(ch)) {
			mara_check_error(mara_lexer_handle_new_line(ctx, lexer, ch));
			continue;
		} else if (mara_lexer_is_paren(ch)) {
			if (ch == '(') {
				*result = mara_lexer_make_token(lexer, MARA_TOK_LEFT_PAREN);
			} else {
				*result = mara_lexer_make_token(lexer, MARA_TOK_RIGHT_PAREN);
			}
			return mara_lexer_consume_char(ctx, lexer);
		} else if (ch == '-') {
			mara_lexer_begin_capture(lexer);
			mara_check_error(mara_lexer_consume_char(ctx, lexer));

			mara_check_error(mara_lexer_peek(ctx, lexer, &ch));
			if (mara_lexer_is_digit(ch)) {
				return mara_lexer_continue_number(ctx, lexer, result);
			} else {
				return mara_lexer_continue_symbol(ctx, lexer, result);
			}
		} else if (mara_lexer_is_digit(ch)) {
			mara_lexer_begin_capture(lexer);
			return mara_lexer_continue_number(ctx, lexer, result);
		} else if (ch == '"') {
			return mara_lexer_handle_string(ctx, lexer, result);
		} else {
			mara_lexer_begin_capture(lexer);
			mara_check_error(mara_lexer_consume_char(ctx, lexer));
			return mara_lexer_continue_symbol(ctx, lexer, result);
		}
	}
}
