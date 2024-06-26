#ifndef MARA_LEXER_H
#define MARA_LEXER_H

#include "internal.h"

typedef struct {
	mara_source_pos_t current_pos;
	mara_source_pos_t capture_start_pos;
	mara_str_t filename;
	mara_reader_t src;
	mara_source_info_t debug_info;
	size_t capture_len;
	bool capturing;
	char read_buf;
	bool buffered;
	char capture_buf[512];
} mara_lexer_t;

typedef enum {
	MARA_TOK_SYMBOL,
	MARA_TOK_STRING,
	MARA_TOK_INT,
	MARA_TOK_REAL,
	MARA_TOK_LEFT_PAREN,
	MARA_TOK_RIGHT_PAREN,
	MARA_TOK_END,
} mara_token_type_t;

typedef struct {
	mara_token_type_t type;
	mara_str_t lexeme;
	mara_source_range_t location;
} mara_token_t;

void
mara_lexer_init(mara_lexer_t* lexer, mara_str_t filename, mara_reader_t src);

mara_error_t*
mara_lexer_next(mara_exec_ctx_t* ctx, mara_lexer_t* lexer, mara_token_t* result);

#endif
