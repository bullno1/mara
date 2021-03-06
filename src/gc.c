#define UGC_IMPLEMENTATION
#include "internal.h"
#include "strpool.h"
#include "string.h"
#include "ptr_map.h"
#include "thread.h"


static size_t
mara_sizeof_gc_obj(mara_gc_header_t* header)
{
	switch(header->obj_type)
	{
		case MARA_GC_STRING:
		case MARA_GC_SYMBOL:
			return mara_sizeof_string(
				BK_CONTAINER_OF(header, mara_string_t, gc_header)
			);
		case MARA_GC_THREAD:
			return mara_sizeof_thread(
				BK_CONTAINER_OF(header, mara_thread_t, gc_header)
			);
		default:
			return 0;
	}
}

static void
mara_gc_scan_root(mara_context_t* ctx)
{
	mara_gc_mark_thread(ctx, ctx->main_thread);
	mara_gc_mark_thread(ctx, ctx->current_thread);
	mara_gc_mark_ptr_map(ctx, &ctx->record_decls);
	// TODO: scan the list of activated threads since they don't have write
	// barriers
}

static void
mara_gc_scan_obj(mara_context_t* ctx, mara_gc_header_t* header)
{
	switch(header->obj_type)
	{
		case MARA_GC_THREAD:
			mara_gc_mark_thread(
				ctx, BK_CONTAINER_OF(header, mara_thread_t, gc_header)
			);
			break;
		case MARA_GC_STRING:
		case MARA_GC_SYMBOL:
			break;
		default:
			MARA_ASSERT(ctx, false, "Unknown object type");
			break;
	}
}

static void
mara_gc_scan(ugc_t* gc, ugc_header_t* obj)
{
	mara_context_t* ctx = BK_CONTAINER_OF(gc, mara_context_t, gc);

	if(obj == NULL)
	{
		mara_gc_scan_root(ctx);
	}
	else
	{
		mara_gc_header_t* header = BK_CONTAINER_OF(
			obj, mara_gc_header_t, ugc_header
		);
		mara_gc_scan_obj(ctx, header);
	}
}

static void
mara_gc_release(ugc_t* gc, ugc_header_t* obj)
{
	mara_context_t* ctx = BK_CONTAINER_OF(gc, mara_context_t, gc);
	mara_gc_header_t* header = BK_CONTAINER_OF(
		obj, mara_gc_header_t, ugc_header
	);

	ctx->gc_mem -= mara_sizeof_gc_obj(header);

	switch(header->obj_type)
	{
		case MARA_GC_SYMBOL:
			mara_strpool_release(
				ctx,
				&ctx->symtab,
				BK_CONTAINER_OF(header, mara_string_t, gc_header)
			);
			break;
		case MARA_GC_STRING:
			mara_release_string(
				ctx, BK_CONTAINER_OF(header, mara_string_t, gc_header)
			);
			break;
		case MARA_GC_THREAD:
			mara_release_thread(
				ctx, BK_CONTAINER_OF(header, mara_thread_t, gc_header)
			);
			break;
		default:
			MARA_ASSERT(ctx, false, "Unknown object type");
			break;
	}
}


void
mara_gc_init(mara_context_t* ctx)
{
	ugc_init(&ctx->gc, mara_gc_scan, mara_gc_release);
}

void
mara_gc_cleanup(mara_context_t* ctx)
{
	ugc_release_all(&ctx->gc);
}

void
mara_gc_mark(mara_context_t* ctx, mara_gc_header_t* header)
{
	if(header != NULL) { ugc_visit(&ctx->gc, &header->ugc_header); }
}

void
mara_gc_register(mara_context_t* ctx, mara_gc_header_t* header)
{
	ugc_register(&ctx->gc, &header->ugc_header);

	ctx->gc_mem += mara_sizeof_gc_obj(header);
}

void
mara_gc_tick(mara_context_t* ctx)
{
	(void)ctx;
}

void
mara_gc_write_barrier(
	mara_context_t* ctx,
	mara_gc_header_t* parent,
	mara_gc_header_t* child
)
{
	if(child == NULL) { return; }

	switch(parent->obj_type)
	{
		case MARA_GC_UPVALUE:
			ugc_write_barrier(
				&ctx->gc,
				UGC_BARRIER_FORWARD,
				&parent->ugc_header, &parent->ugc_header
			);
			break;
		case MARA_GC_THREAD:
		case MARA_GC_LIST:
			ugc_write_barrier(
				&ctx->gc,
				UGC_BARRIER_BACKWARD,
				&parent->ugc_header, &parent->ugc_header
			);
			break;
		default:
			MARA_ASSERT(ctx, false, "Invalid write barrier");
			break;
	}
}
