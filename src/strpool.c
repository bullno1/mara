#include "strpool.h"
#include <string.h>
#include <bk/allocator.h>
#include <bk/assert.h>
#include <robinhoodhash/robinhoodhash.h>
#define XXH_INLINE_ALL
#include <xxHash/xxhash.h>

#define mara_strpool_setvalue(u, index, key, val) u->strings[index] = value
#define mara_strpool_setnil(u, index) u->strings[index] = NULL;
#define mara_strpool_nilvalue(u) NULL
#define mara_strpool_getvalue(u, index) u->strings[index]
#define mara_strpool_getkey(u, index) mara_strpool_key(u->strings[index])
#define mara_strpool_keysequal(u, key1, key2) mara_strpool_key_cmp(key1, key2)
#define mara_strpool_isnil(u, index) (u->strings[index] == NULL)
#define mara_strpool_n_elem(u) u->capacity + 1
#define mara_strpool_overflow(u) MARA_ASSERT(ctx, 0, "Hash overflow")
#define mara_strpool_removefailed(u,key)
#define mara_strpool_swap(u, index1, index2) \
	do { \
		mara_string_t* tmp = u->strings[index1]; \
		u->strings[index1] = u->strings[index2]; \
		u->strings[index2] = tmp; \
	} while(0);

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
	return key1.ref.length == key2.ref.length
		&& memcmp(key1.ref.ptr, key2.ref.ptr, key1.ref.length) == 0;
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

static mara_strpool_t
mara_strpool_create(mara_ctx_t* ctx, size_t capacity)
{
	size_t malloc_size = (capacity + 1) * sizeof(mara_string_t*);
	mara_string_t** strings = mara_malloc(ctx, malloc_size);
	memset(strings, 0, malloc_size);

	return (mara_strpool_t){
		.strings = strings,
		.size = 0,
		.capacity = capacity
	};
}


static void
mara_mark_string(mara_ctx_t* ctx, mara_gc_header_t* header)
{
	(void)ctx;
	(void)header;
}

static void
mara_free_string(mara_ctx_t* ctx, mara_gc_header_t* header)
{
	mara_string_t* string = BK_CONTAINER_OF(header, mara_string_t, gc_header);
	mara_strpool_key_t key = mara_strpool_key(string);
	ROBINHOOD_HASH_DEL(mara_strpool, (&ctx->strpool), key);
	mara_free(ctx, string);
}


static mara_gc_info_t mara_string_gc_info = {
	.mark_fn = mara_mark_string,
	.free_fn = mara_free_string,
};


void
mara_strpool_init(mara_ctx_t* ctx, mara_strpool_t* strpool)
{
	*strpool = mara_strpool_create(ctx, MARA_HASH_INITIAL_CAPACITY);
	ROBINHOOD_HASH_CLEAR(mara_strpool, strpool);
}

void
mara_strpool_cleanup(mara_ctx_t* ctx, mara_strpool_t* strpool)
{
	mara_free(ctx, strpool->strings);
}

mara_string_t*
mara_strpool_alloc(
	mara_ctx_t* ctx,
	mara_strpool_t* strpool,
	mara_string_ref_t string
)
{
	mara_hash_t hash = MARA_HASH(string.ptr, string.length, (uintptr_t)strpool);
	mara_string_t* value;
	mara_strpool_key_t key = {
		.hash = hash,
		.ref = string
	};
	ROBINHOOD_HASH_GET(mara_strpool, strpool, key, value);

	if(value != NULL) { return value; }

	// Resize if we are at capacity
	size_t capacity = strpool->capacity;
	size_t max_size = (size_t)((double)capacity * MARA_HASH_LOAD_FACTOR);
	if(strpool->size >= max_size)
	{
		mara_strpool_t new_strpool = mara_strpool_create(ctx, capacity * 2);

		for(size_t i = 0; i < capacity + 1; ++i)
		{
			mara_string_t* string = strpool->strings[i];
			mara_strpool_key_t key = mara_strpool_key(string);
			if(string != NULL)
			{
				ROBINHOOD_HASH_SET(mara_strpool, (&new_strpool), key, string);
			}
		}

		mara_strpool_cleanup(ctx, strpool);
		*strpool = new_strpool;
	}

	mara_string_t* pooled_string = mara_gc_malloc(
		ctx, &mara_string_gc_info, sizeof(mara_string_t) + string.length + 1
	);
	pooled_string->hash = hash;
	pooled_string->length = string.length;
	memcpy(pooled_string->data, string.ptr, string.length);
	pooled_string->data[string.length] = '\0';

	ROBINHOOD_HASH_SET(mara_strpool, strpool, key, string);

	return pooled_string;
}
