#include "internal.h"


mara_string_t*
mara_alloc_string_hashed(
	mara_context_t* ctx,
	mara_string_ref_t value,
	mara_hash_t hash
);

mara_string_t*
mara_alloc_string(mara_context_t* ctx, mara_string_ref_t value);

static inline size_t
mara_sizeof_string(mara_string_t* string)
{
	return sizeof(*string) + sizeof(string->length) + 1;
}

static inline void
mara_release_string(mara_context_t* ctx, mara_string_t* string)
{
	mara_free(ctx, string);
}
