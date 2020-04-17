#ifndef MARA_LEXER_H
#define MARA_LEXER_H

#include "internal.h"


typedef struct mara_lexer_s mara_lexer_t;


void
mara_lexer_init(
	mara_ctx_t* ctx,
	mara_lexer_t* lexer,
	struct bk_file_s* file,
);

void
mara_lexer_cleanup(mara_lexer_t* lexer);

mara_exec_t
mara_lexer_next_token(mara_lexer_t* lexer);
