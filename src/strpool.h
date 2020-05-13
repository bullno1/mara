#ifndef MARA_STRPOOL_H
#define MARA_STRPOOL_H

#include "internal.h"


void
mara_strpool_init(mara_context_t* ctx, mara_strpool_t* strpool);

void
mara_strpool_cleanup(mara_context_t* ctx, mara_strpool_t* strpool);

mara_string_t*
mara_strpool_alloc(
	mara_context_t* ctx,
	mara_strpool_t* strpool,
	mara_string_ref_t string
);

void
mara_strpool_release(
	mara_context_t* ctx,
	mara_strpool_t* strpool,
	mara_string_t* string
);

#endif
