#ifndef MARA_INTERNAL_H
#define MARA_INTERNAL_H

#include <mara.h>
#include <ugc/ugc.h>
#include <bk/allocator.h>

#if INTPTR_MAX == INT32_MAX
#	define MARA_HASH_BITS 32
#	define MARA_HASH_TYPE uint32_t
#elif INTPTR_MAX == INT64_MAX
#	define MARA_HASH_BITS 64
#	define MARA_HASH_TYPE uint64_t
#else
#	define MARA_HASH_BITS 32
#	define MARA_HASH_TYPE uint32_t
#endif

#ifndef MARA_HASH_INITIAL_CAPACITY
#define MARA_HASH_INITIAL_CAPACITY 4
#endif

#ifndef MARA_HASH_LOAD_FACTOR
#define MARA_HASH_LOAD_FACTOR 0.9
#endif

#define MARA_HASH MARA_HASH2(MARA_HASH_BITS)
#define MARA_HASH2(X) MARA_HASH3(X)
#define MARA_HASH3(B) XXH##B

#define MARA_ASSERT(ctx, condition, message) \
	do { \
		if(!(condition)) { ctx->config.panic_handler(ctx, message); } \
	} while(0);

typedef uint16_t mara_source_addr_t;
typedef MARA_HASH_TYPE mara_hash_t;
typedef struct mara_string_s mara_string_t;
typedef struct mara_gc_header_s mara_gc_header_t;
typedef struct mara_gc_info_s mara_gc_info_t;
typedef struct mara_source_coord_s mara_source_coord_t;
typedef struct mara_source_location_s mara_source_location_t;
typedef struct mara_strpool_s mara_strpool_t;
typedef void(*mara_gc_visit_fn_t)(mara_ctx_t* ctx, mara_gc_header_t* header);

struct mara_source_coord_s
{
	mara_source_addr_t line;
	mara_source_addr_t column;
};

struct mara_source_location_s
{
	mara_string_t* file;
	mara_source_coord_t start;
	mara_source_coord_t end;
};

struct mara_strpool_s
{
	mara_string_t** strings;
	size_t size;
	size_t capacity;
};

struct mara_ctx_s
{
	bk_allocator_t allocator;
	mara_ctx_config_t config;
	ugc_t gc;
	mara_strpool_t strpool;
};

struct mara_gc_info_s
{
	mara_gc_visit_fn_t mark_fn;
	mara_gc_visit_fn_t free_fn;
};

struct mara_gc_header_s
{
	ugc_header_t ugc_header;
	mara_gc_info_t* gc_info;
};

struct mara_string_s
{
	mara_gc_header_t gc_header;
	mara_hash_t hash;
	size_t length;
	BK_FLEXIBLE_ARRAY_MEMBER(char, data);
};


void*
mara_gc_malloc(mara_ctx_t* ctx, mara_gc_info_t* gc_info, size_t size);

void
mara_gc_mark(mara_ctx_t* ctx, mara_gc_header_t* header);


static inline void*
mara_malloc(mara_ctx_t* ctx, size_t size)
{
	return bk_malloc(&ctx->allocator, size);
}

static inline void
mara_free(mara_ctx_t* ctx, void* ptr)
{
	bk_free(&ctx->allocator, ptr);
}

#endif
