#ifndef MARA_LEXER_H
#define MARA_LEXER_H

#include "internal.h"


typedef struct mara_lexer_s mara_lexer_t;
typedef struct mara_token_s mara_token_t;


#define MARA_LEXER(X) \
	X(MARA_LEXER_OK) \
	X(MARA_LEXER_END) \
	X(MARA_LEXER_IO_ERROR) \
	X(MARA_LEXER_BAD_STRING) \
	X(MARA_LEXER_BAD_NUMBER) \

BK_ENUM(mara_lexer_status_t, MARA_LEXER)

#define MARA_TOKEN(X) \
	X(MARA_TOKEN_LPAREN) \
	X(MARA_TOKEN_RPAREN) \
	X(MARA_TOKEN_QUOTE) \
	X(MARA_TOKEN_QUASIQUOTE) \
	X(MARA_TOKEN_UNQUOTE) \
	X(MARA_TOKEN_UNQUOTE_SPLICING) \
	X(MARA_TOKEN_STRING) \
	X(MARA_TOKEN_SYMBOL) \
	X(MARA_TOKEN_NUMBER)

BK_ENUM(mara_token_type_t, MARA_TOKEN)

struct mara_token_s
{
	mara_token_type_t type;
	mara_string_ref_t lexeme;
	mara_source_range_t location;
};

struct mara_lexer_s
{
	BK_ARRAY(char) capture_buff;
	char read_buff;
	bool buffered;
	bool eos;
	mara_source_coord_t location;
	struct bk_file_s* input;
};


void
mara_lexer_init(mara_ctx_t* ctx, mara_lexer_t* lexer);

void
mara_lexer_cleanup(mara_lexer_t* lexer);

void
mara_lexer_reset(mara_lexer_t* lexer, struct bk_file_s* file);

mara_lexer_status_t
mara_lexer_next_token(mara_lexer_t* lexer, mara_token_t* token);

#endif
