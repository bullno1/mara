#include "lexer.h"
#include <ctype.h>
#include <bk/array.h>
#include <bk/fs.h>


static bool
mara_lexer_is_separator(char ch)
{
	return isspace(ch) || ch == ')' || ch == '(' || ch == ';' || ch == '"'
		|| ch == '\'' || ch == '`' || ch == ',';
}

static mara_lexer_status_t
mara_lexer_peek_char(mara_lexer_t* lexer, char* ch)
{
	if(lexer->buffered)
	{
		*ch = lexer->read_buff;
		return MARA_LEXER_OK;
	}

	size_t len = 1;

	if(bk_fread(lexer->input, &lexer->read_buff, &len) != 0)
	{
		return MARA_LEXER_IO_ERROR;
	}
	else if(len == 0)
	{
		return MARA_LEXER_END;
	}
	else
	{
		lexer->buffered = true;
		*ch = lexer->read_buff;
		return MARA_LEXER_OK;
	}
}

static void
mara_lexer_consume_char(mara_lexer_t* lexer)
{
	lexer->buffered = false;
	bk_array_push(lexer->capture_buff, lexer->read_buff);
	++lexer->location.column;
}

static void
mara_lexer_reset_capture(mara_lexer_t* lexer)
{
	bk_array_clear(lexer->capture_buff);
}

static mara_lexer_status_t
mara_lexer_make_token(
	mara_lexer_t* lexer, mara_token_t* token, mara_token_type_t token_type
)
{
	token->type = token_type;
	token->location.end = lexer->location;
	--token->location.end.column;
	token->lexeme = (mara_string_ref_t){
		.length = bk_array_len(lexer->capture_buff),
		.ptr = lexer->capture_buff,
	};

	return MARA_LEXER_OK;
}

static mara_lexer_status_t
mara_lexer_error(
	mara_lexer_t* lexer, mara_token_t* token, mara_lexer_status_t status
)
{
	mara_lexer_make_token(lexer, token, MARA_TOKEN_SYMBOL);
	return status;
}

static mara_lexer_status_t
mara_lexer_scan_string(mara_lexer_t* lexer, mara_token_t* token)
{
	mara_lexer_reset_capture(lexer);
	char previous_char = '"';
	char ch;

	while(true)
	{
		mara_lexer_status_t status = mara_lexer_peek_char(lexer, &ch);
		if(status == MARA_LEXER_IO_ERROR)
		{
			return status;
		}
		else if(status == MARA_LEXER_END)
		{
			return mara_lexer_error(
				lexer, token, MARA_LEXER_BAD_STRING
			);
		}
		else if(ch == '"' && previous_char != '\\')
		{
			mara_lexer_consume_char(lexer);
			mara_lexer_make_token(lexer, token, MARA_TOKEN_STRING);
			--token->lexeme.length; // Exclude the ending quote '"'
			return MARA_LEXER_OK;
		}
		else if(ch == '\n' || ch == '\r')
		{
			return mara_lexer_error(
				lexer, token, MARA_LEXER_BAD_STRING
			);
		}
		else
		{
			mara_lexer_consume_char(lexer);
		}

		if(ch == '\\' && previous_char == '\\')
		{
			previous_char = 0;
		}
		else
		{
			previous_char = ch;
		}
	}

	return MARA_LEXER_IO_ERROR;
}

static mara_lexer_status_t
mara_lexer_scan_number(mara_lexer_t* lexer, mara_token_t* token)
{
	char ch;
	bool found_point = false;

	while(true)
	{
		mara_lexer_status_t status = mara_lexer_peek_char(lexer, &ch);
		if(status == MARA_LEXER_IO_ERROR)
		{
			return status;
		}
		else if(status == MARA_LEXER_END || mara_lexer_is_separator(ch))
		{
			return mara_lexer_make_token(lexer, token, MARA_TOKEN_NUMBER);
		}
		else if(ch == '.')
		{
			mara_lexer_consume_char(lexer);

			if(found_point)
			{
				return mara_lexer_error(
					lexer, token, MARA_LEXER_BAD_NUMBER
				);
			}
			else
			{
				found_point = true;
				continue;
			}
		}
		else if(isdigit(ch))
		{
			mara_lexer_consume_char(lexer);
			continue;
		}
		else
		{
			return mara_lexer_error(
				lexer, token, MARA_LEXER_BAD_NUMBER
			);
		}
	}

	return MARA_LEXER_IO_ERROR;
}

static mara_lexer_status_t
mara_lexer_scan_symbol(mara_lexer_t* lexer, mara_token_t* token)
{
	char ch;
	while(true)
	{
		mara_lexer_status_t status = mara_lexer_peek_char(lexer, &ch);
		if(status == MARA_LEXER_IO_ERROR)
		{
			return status;
		}
		else if(status == MARA_LEXER_END || mara_lexer_is_separator(ch))
		{
			return mara_lexer_make_token(lexer, token, MARA_TOKEN_SYMBOL);
		}
		else
		{
			mara_lexer_consume_char(lexer);
		}
	}

	return MARA_LEXER_IO_ERROR;
}

void
mara_lexer_init(mara_ctx_t* ctx, mara_lexer_t* lexer)
{
	*lexer = (mara_lexer_t) {
		.capture_buff = bk_array_create(&ctx->allocator, char, 8),
	};
}

void
mara_lexer_cleanup(mara_lexer_t* lexer)
{
	bk_array_destroy(lexer->capture_buff);
}

void
mara_lexer_reset(mara_lexer_t* lexer, struct bk_file_s* file)
{
	lexer->location.line = 1;
	lexer->location.column = 1;
	lexer->read_buff = 0;
	lexer->buffered = false;
	lexer->input = file;
	bk_array_clear(lexer->capture_buff);
}

mara_lexer_status_t
mara_lexer_next_token(mara_lexer_t* lexer, mara_token_t* token)
{
	char ch;
	while(true)
	{
		mara_lexer_status_t status = mara_lexer_peek_char(lexer, &ch);
		if(status != MARA_LEXER_OK) { return status; }

		token->location.start = lexer->location;
		mara_lexer_consume_char(lexer);

		switch(ch)
		{
			case ' ':
			case '\t':
				mara_lexer_reset_capture(lexer);
				continue;
			case '\r':
				mara_lexer_reset_capture(lexer);
				++lexer->location.line;
				lexer->location.column = 1;

				status = mara_lexer_peek_char(lexer, &ch);
				if(status != MARA_LEXER_OK) { return status; }

				if(ch == '\n')
				{
					mara_lexer_consume_char(lexer);
				}
				continue;
			case '\n':
				mara_lexer_reset_capture(lexer);
				++lexer->location.line;
				lexer->location.column = 1;
				continue;
			case '(':
				return mara_lexer_make_token(lexer, token, MARA_TOKEN_LPAREN);
			case ')':
				return mara_lexer_make_token(lexer, token, MARA_TOKEN_RPAREN);
			case '\'':
				return mara_lexer_make_token(lexer, token, MARA_TOKEN_QUOTE);
			case '`':
				return mara_lexer_make_token(
					lexer, token, MARA_TOKEN_QUASIQUOTE
				);
			case ',':
				status = mara_lexer_peek_char(lexer, &ch);

				if(status == MARA_LEXER_IO_ERROR)
				{
					return status;
				}
				else if(status == MARA_LEXER_END)
				{
					return mara_lexer_make_token(
						lexer, token, MARA_TOKEN_UNQUOTE
					);
				}
				else if(ch == '@')
				{
					mara_lexer_consume_char(lexer);
					return mara_lexer_make_token(
						lexer, token, MARA_TOKEN_UNQUOTE_SPLICING
					);
				}
				else
				{
					return mara_lexer_make_token(
						lexer, token, MARA_TOKEN_UNQUOTE
					);
				}
			case ';':
				while(true)
				{
					status = mara_lexer_peek_char(lexer, &ch);
					if(status != MARA_LEXER_OK) { return status; }

					if(ch == '\r' || ch == '\n')
					{
						break;
					}
					else
					{
						mara_lexer_consume_char(lexer);
					}
				}
				continue;
			case '"':
				return mara_lexer_scan_string(lexer, token);
			case '-':
				status = mara_lexer_peek_char(lexer, &ch);

				if(status == MARA_LEXER_IO_ERROR)
				{
					return status;
				}
				else if(status == MARA_LEXER_END)
				{
					return mara_lexer_make_token(
						lexer, token, MARA_TOKEN_SYMBOL
					);
				}
				else if(isdigit(ch))
				{
					return mara_lexer_scan_number(lexer, token);
				}
				else if(mara_lexer_is_separator(ch))
				{
					return mara_lexer_make_token(
						lexer, token, MARA_TOKEN_SYMBOL
					);
				}
				else
				{
					return mara_lexer_scan_symbol(lexer, token);
				}
			default:
				if(isdigit(ch))
				{
					return mara_lexer_scan_number(lexer, token);
				}
				else
				{
					return mara_lexer_scan_symbol(lexer, token);
				}
		}
	}

	return MARA_LEXER_END;
}
