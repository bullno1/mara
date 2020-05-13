#include "string.h"
#define XXH_INLINE_ALL
#include <xxHash/xxhash.h>


mara_string_t*
mara_alloc_string(
	mara_context_t* ctx,
	mara_string_ref_t value
)
{
	return mara_alloc_string_hashed(
		ctx,
		value,
		MARA_HASH(ctx, value.ptr, value.length)
	);
}

mara_string_t*
mara_alloc_string_hashed(
	mara_context_t* ctx,
	mara_string_ref_t value,
	mara_hash_t hash
)
{
	mara_string_t* string = mara_malloc(
		ctx, sizeof(mara_string_t) + value.length + 1
	);
	string->hash = hash;
	string->length = value.length;
	memcpy(string->data, value.ptr, value.length);
	string->data[value.length] = '\0';

	return string;
}
