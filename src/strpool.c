#include "strpool.h"
#include "string.h"
#include <bk/allocator.h>
#include <bk/assert.h>
#include "vendor/robinhoodhash.h"
#define XXH_INLINE_ALL
#include <xxHash/xxhash.h>

#define mara_strpool_setvalue(u, index, key, val) u->strings[index] = val
#define mara_strpool_setnil(u, index) u->strings[index] = NULL;
#define mara_strpool_nilvalue(u) NULL;
#define mara_strpool_getvalue(u, index) u->strings[index]
#define mara_strpool_getkey(u, index) mara_strpool_key(u->strings[index])
#define mara_strpool_keysequal(u, key1, key2) mara_strpool_key_cmp(key1, key2)
#define mara_strpool_isnil(u, index) (u->strings[index] == NULL)
#define mara_strpool_n_elem(u) u->capacity + 1
#define mara_strpool_size(u) u->size
#define mara_strpool_overflow(u) MARA_ASSERT(ctx, 0, "Hash overflow")
#define mara_strpool_removefailed(u,key) removed = 0;
#define mara_strpool_swap(u, index1, index2) \
	MARA_HASH_SWAP(mara_string_t*, u->strings, index1, index2)

typedef struct mara_strpool_key_s
{
	mara_hash_t hash;
	mara_string_ref_t ref;
} mara_strpool_key_t;


static inline mara_hash_t
mara_strpool_getbucket(
	mara_strpool_t* strpool,
	mara_strpool_key_t key
)
{
	return (key.hash & (strpool->capacity - 1)) + 1;
}

static inline bool
mara_strpool_key_cmp(mara_strpool_key_t key1, mara_strpool_key_t key2)
{
	return mara_string_ref_equal(key1.ref, key2.ref);
}


static inline mara_strpool_key_t
mara_strpool_key(mara_string_t* string)
{
	return (mara_strpool_key_t) {
		.hash = string->hash,
		.ref = {
			.length = string->length,
			.ptr = string->data
		}
	};
}

static void
mara_strpool_init_internal(
	mara_context_t* ctx,
	mara_strpool_t* strpool,
	size_t capacity
)
{
	size_t malloc_size = (capacity + 1) * sizeof(mara_string_t*);
	strpool->strings = mara_malloc(ctx, malloc_size);
	strpool->size = 0;
	strpool->capacity = capacity;

	ROBINHOOD_HASH_CLEAR(mara_strpool, strpool);
}


void
mara_strpool_init(mara_context_t* ctx, mara_strpool_t* strpool)
{
	mara_strpool_init_internal(ctx, strpool, MARA_HASH_INITIAL_CAPACITY);
}

void
mara_strpool_cleanup(mara_context_t* ctx, mara_strpool_t* strpool)
{
	// For testability and symmetry sake, we loop over and release all strings
	// In practice, the pool should be empty due to GC
	while(strpool->size > 0)
	{
		ROBINHOOD_HASH_FOREACH(mara_strpool, strpool, i)
		{
			mara_string_t* string = strpool->strings[i];
			mara_strpool_release(ctx, strpool, string);
			break;
		}
	}

	mara_free(ctx, strpool->strings);
}

mara_string_t*
mara_strpool_alloc(
	mara_context_t* ctx,
	mara_strpool_t* strpool,
	mara_string_ref_t string
)
{
	mara_hash_t hash = MARA_HASH(ctx, string.ptr, string.length);
	mara_string_t* value;
	mara_strpool_key_t key = {
		.hash = hash,
		.ref = string
	};
	ROBINHOOD_HASH_GET(mara_strpool, strpool, key, value);

	if(value != NULL) { return value; }

	size_t capacity = strpool->capacity;
	size_t max_size = (size_t)((double)capacity * MARA_HASH_LOAD_FACTOR);
	if(strpool->size >= max_size)
	{
		mara_strpool_t new_strpool;
		mara_strpool_init_internal(ctx, &new_strpool, capacity * 2);

		ROBINHOOD_HASH_FOREACH(mara_strpool, strpool, i)
		{
			mara_string_t* string = strpool->strings[i];
			mara_strpool_key_t key = mara_strpool_key(string);
			ROBINHOOD_HASH_SET(mara_strpool, (&new_strpool), key, string);
		}

		mara_free(ctx, strpool->strings);
		*strpool = new_strpool;
	}

	mara_string_t* pooled_string = mara_alloc_string_hashed(ctx, string, hash);
	ROBINHOOD_HASH_SET(mara_strpool, strpool, key, pooled_string);

	return pooled_string;
}

void
mara_strpool_release(
	mara_context_t* ctx,
	mara_strpool_t* strpool,
	mara_string_t* string
)
{
	int removed = 1;

	mara_strpool_key_t key = mara_strpool_key(string);
	ROBINHOOD_HASH_DEL(mara_strpool, strpool, key);
	MARA_ASSERT(ctx, removed, "Invalid mara_strpool_release");
	mara_release_string(ctx, string);
}
