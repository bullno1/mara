#include "internal.h"
#include "gc.h"
#include "strpool.h"
#include "ptr_map.h"
#include "thread.h"


static void*
mara_realloc(bk_allocator_t* allocator, void* ptr, size_t size)
{
	mara_context_t* ctx = BK_CONTAINER_OF(allocator, mara_context_t, allocator);
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

	mara_gc_tick(ctx);

	return result;
}


mara_context_t*
mara_create_context(const mara_context_config_t* config)
{
	mara_context_t* ctx = BK_UNSAFE_NEW(config->allocator, mara_context_t);
	if(ctx == NULL)
	{
		config->panic_handler(NULL, "Out of memory", __FILE__, __LINE__);
	}

	*ctx = (mara_context_t) {
		.allocator = {
			.realloc = mara_realloc
		},
		.config = *config,
	};

	mara_strpool_init(ctx, &ctx->symtab);

	ctx->current_thread = ctx->main_thread =
		mara_alloc_thread(ctx, &config->main_thread_config);

	mara_ptr_map_init(ctx, &ctx->record_decls);

	mara_gc_init(ctx);

	return ctx;
}

void
mara_destroy_context(mara_context_t* ctx)
{
	mara_gc_cleanup(ctx);
	mara_ptr_map_cleanup(ctx, &ctx->record_decls);
	mara_release_thread(ctx, ctx->main_thread);
	mara_strpool_cleanup(ctx, &ctx->symtab);
	bk_free(ctx->config.allocator, ctx);
}
