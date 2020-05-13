#define UGC_IMPLEMENTATION
#include "internal.h"
#include "strpool.h"


static void
mara_gc_scan(ugc_t* gc, ugc_header_t* obj)
{
	mara_context_t* ctx = BK_CONTAINER_OF(gc, mara_context_t, gc);

	if(obj == NULL)
	{
	}
	else
	{
		mara_gc_header_t* header = BK_CONTAINER_OF(
			obj, mara_gc_header_t, ugc_header
		);

		switch(header->obj_type)
		{
			case MARA_GC_STRING:
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
			mara_strpool_release(ctx, &ctx->symtab, (mara_string_t*)header);
			break;
		case MARA_GC_STRING:
			mara_free(ctx, header);
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
	ugc_visit(&ctx->gc, &header->ugc_header);
}
