#include "internal.h"
#include "string.h"


static inline mara_value_t*
mara_stack_alloc(mara_context_t* ctx)
{
	mara_thread_t* current_thread = ctx->current_thread;
	mara_stack_frame_t* current_stack_frame = current_thread->frame_pointer;
	mara_value_t* new_val = --current_stack_frame->stack_pointer;

	MARA_ASSERT(
		ctx,
		(uintptr_t)new_val >= (uintptr_t)(current_stack_frame + 1),
		"Stack overflow"
	);

	return new_val;
}


MARA_DECL mara_index_t
mara_stack_len(mara_context_t* ctx)
{
	mara_thread_t* current_thread = ctx->current_thread;
	mara_stack_frame_t* current_stack_frame = current_thread->frame_pointer;
	return current_stack_frame->base_pointer - current_stack_frame->stack_pointer;
}

MARA_DECL void
mara_push_null(mara_context_t* ctx)
{
	mara_value_set_null(mara_stack_alloc(ctx));
}

MARA_DECL void
mara_push_number(mara_context_t* ctx, mara_number_t num)
{
	mara_value_set_number(mara_stack_alloc(ctx), num);
}

MARA_DECL void
mara_push_string(mara_context_t* ctx, mara_string_ref_t str)
{
	mara_string_t* string = mara_alloc_string(ctx, str);
	string->gc_header.obj_type = MARA_GC_STRING;

	mara_value_set_gc_obj(mara_stack_alloc(ctx), &string->gc_header);

	mara_gc_register(ctx, &string->gc_header);
}
