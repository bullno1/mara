#define UGC_IMPLEMENTATION
#include "internal.h"
#include "strpool.h"


static void
mara_gc_scan(ugc_t* gc, ugc_header_t* obj)
{
	mara_ctx_t* ctx = BK_CONTAINER_OF(gc, mara_ctx_t, gc);
	if(obj == NULL)
	{
	}
	else
	{
		mara_gc_header_t* header = BK_CONTAINER_OF(
			obj, mara_gc_header_t, ugc_header
		);
		header->gc_info->mark_fn(ctx, header);
	}
}

static void
mara_gc_release(ugc_t* gc, ugc_header_t* obj)
{
	mara_ctx_t* ctx = BK_CONTAINER_OF(gc, mara_ctx_t, gc);
	mara_gc_header_t* header = BK_CONTAINER_OF(
		obj, mara_gc_header_t, ugc_header
	);
	header->gc_info->free_fn(ctx, header);
}

static void*
mara_realloc(bk_allocator_t* allocator, void* ptr, size_t size)
{
	mara_ctx_t* ctx = BK_CONTAINER_OF(allocator, mara_ctx_t, allocator);
	void* result = bk_unsafe_realloc(ctx->config.allocator, ptr, size);

	// Emergency GC
	if(size != 0 && result == NULL)
	{
		if(ctx->gc.state == UGC_SWEEP) { ugc_collect(&ctx->gc); }
		ugc_collect(&ctx->gc);

		// Retry
		result = bk_unsafe_realloc(ctx->config.allocator, ptr, size);
		MARA_ASSERT(ctx, size == 0 || result != NULL, "Out of memory");
	}

	return result;
}


mara_ctx_t*
mara_create_ctx(const mara_ctx_config_t* config)
{
	mara_ctx_t* ctx = BK_UNSAFE_NEW(config->allocator, mara_ctx_t);
	if(ctx == NULL)
	{
		config->panic_handler(NULL, "Out of memory");
	}

	*ctx = (mara_ctx_t) {
		.allocator = {
			.realloc = mara_realloc
		},
		.config = *config,
	};

	mara_strpool_init(ctx, &ctx->strpool);
	ugc_init(&ctx->gc, mara_gc_scan, mara_gc_release);

	return ctx;
}

void
mara_destroy_ctx(mara_ctx_t* ctx)
{
	ugc_release_all(&ctx->gc);
	mara_strpool_cleanup(ctx, &ctx->strpool);
	bk_free(ctx->config.allocator, ctx);
}

void*
mara_gc_malloc(mara_ctx_t* ctx, mara_gc_info_t* gc_info, size_t size)
{
	MARA_ASSERT(ctx, size > sizeof(mara_gc_header_t), "Invalid mara_gc_malloc");

	mara_gc_header_t* obj = mara_malloc(ctx, size);
	obj->gc_info = gc_info;
	ugc_register(&ctx->gc, &obj->ugc_header);

	return obj;
}

void
mara_gc_mark(mara_ctx_t* ctx, mara_gc_header_t* header)
{
	ugc_visit(&ctx->gc, &header->ugc_header);
}
