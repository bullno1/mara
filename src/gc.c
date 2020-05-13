#define UGC_IMPLEMENTATION
#include "internal.h"
#include "strpool.h"
#include "string.h"
#include "thread.h"


static void
mara_gc_scan(ugc_t* gc, ugc_header_t* obj)
{
	mara_context_t* ctx = BK_CONTAINER_OF(gc, mara_context_t, gc);

	if(obj == NULL)
	{
		mara_gc_mark_thread(ctx, ctx->main_thread);
		mara_gc_mark_thread(ctx, ctx->current_thread);
	}
	else
	{
		mara_gc_header_t* header = BK_CONTAINER_OF(
			obj, mara_gc_header_t, ugc_header
		);

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
}

static void
mara_gc_release(ugc_t* gc, ugc_header_t* obj)
{
	mara_context_t* ctx = BK_CONTAINER_OF(gc, mara_context_t, gc);
	mara_gc_header_t* header = BK_CONTAINER_OF(
		obj, mara_gc_header_t, ugc_header
	);

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
