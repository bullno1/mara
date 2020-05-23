#include "internal.h"
#include "string.h"
#include "strpool.h"


static inline mara_value_t*
mara_stack_alloc(mara_context_t* ctx)
{
	mara_stack_frame_t* current_frame = ctx->current_thread->frame_pointer;

	MARA_ASSERT(
		ctx,
		(uintptr_t)current_frame->stack_pointer >= (uintptr_t)(current_frame + 1) &&
		(current_frame->base_pointer - current_frame->stack_pointer) <= MARA_STACK_MAX,
		"Stack overflow"
	);

	return current_frame->stack_pointer--;
}


mara_value_t*
mara_stack_addr(mara_context_t* ctx, mara_index_t index)
{
	mara_value_t* result = NULL;
	mara_stack_frame_t* current_frame = ctx->current_thread->frame_pointer;

	if(index >= 0)
	{
		result = current_frame->base_pointer - index;
	}
	else
	{
		result = current_frame->stack_pointer - index;
	}

	MARA_ASSERT(
		ctx,
		current_frame->stack_pointer < result
		&& result <= current_frame->base_pointer,
		"Invalid stack index"
	);

	return result;
}

mara_index_t
mara_stack_len(mara_context_t* ctx)
{
	mara_stack_frame_t* current_frame = ctx->current_thread->frame_pointer;
	return current_frame->base_pointer - current_frame->stack_pointer;
}

bool
mara_is_null(mara_context_t* ctx, mara_index_t index)
{
	return mara_value_type_check(mara_stack_addr(ctx, index), MARA_VAL_NULL);
}

bool
mara_is_bool(mara_context_t* ctx, mara_index_t index)
{
	return mara_value_type_check(mara_stack_addr(ctx, index), MARA_VAL_BOOL);
}

bool
mara_is_number(mara_context_t* ctx, mara_index_t index)
{
	return mara_value_type_check(mara_stack_addr(ctx, index), MARA_VAL_NUMBER);
}

bool
mara_is_string(mara_context_t* ctx, mara_index_t index)
{
	mara_value_t* value = mara_stack_addr(ctx, index);
	return mara_value_type_check(value, MARA_VAL_GC_OBJ)
		&& mara_value_as_gc_obj(value)->obj_type == MARA_GC_STRING;
}

bool
mara_is_symbol(mara_context_t* ctx, mara_index_t index)
{
	mara_value_t* value = mara_stack_addr(ctx, index);
	return mara_value_type_check(value, MARA_VAL_GC_OBJ)
		&& mara_value_as_gc_obj(value)->obj_type == MARA_GC_SYMBOL;
}

void
mara_push_null(mara_context_t* ctx)
{
	mara_value_set_null(mara_stack_alloc(ctx));
}

void
mara_push_number(mara_context_t* ctx, mara_number_t num)
{
	mara_value_set_number(mara_stack_alloc(ctx), num);
}

void
mara_push_string(mara_context_t* ctx, mara_string_ref_t str)
{
	mara_string_t* string = mara_alloc_string(ctx, str);
	string->gc_header.obj_type = MARA_GC_STRING;
	mara_gc_register(ctx, &string->gc_header);

	mara_push_gc_obj(ctx, &string->gc_header);
}

void
mara_make_symbol(mara_context_t* ctx, mara_index_t index)
{
	mara_value_t* value = mara_stack_addr(ctx, index);

	MARA_ASSERT(
		ctx,
		mara_value_type_check(value, MARA_VAL_GC_OBJ),
		"Expecting string or symbol"
	);

	mara_gc_header_t* header = mara_value_as_gc_obj(value);
	MARA_ASSERT(
		ctx,
		header->obj_type == MARA_GC_STRING
		|| header->obj_type == MARA_GC_SYMBOL,
		"Expecting string or symbol"
	);

	if(header->obj_type == MARA_GC_SYMBOL)
	{
		*mara_stack_alloc(ctx) = *value;
	}
	else
	{
		mara_string_t* string = BK_CONTAINER_OF(header, mara_string_t, gc_header);
		mara_string_ref_t ref = {
			.ptr = string->data,
			.length = string->length,
		};
		mara_string_t* symbol = mara_strpool_alloc(ctx, &ctx->symtab, ref);
		symbol->gc_header.obj_type = MARA_GC_SYMBOL;
		mara_gc_register(ctx, &symbol->gc_header);

		mara_push_gc_obj(ctx, &symbol->gc_header);
	}
}

void
mara_dup(mara_context_t* ctx, mara_index_t index)
{
	*mara_stack_alloc(ctx) = *mara_stack_addr(ctx, index);
}

mara_index_t
mara_upvalue_index(mara_context_t* ctx, mara_index_t upvalue);

void
mara_pop(mara_context_t* ctx, mara_index_t n)
{
	MARA_ASSERT(ctx, 0 <= n && n <= mara_stack_len(ctx), "Invalid argument");

	mara_stack_frame_t* current_frame = ctx->current_thread->frame_pointer;
	current_frame->stack_pointer += n;
}

void
mara_replace(mara_context_t* ctx, mara_index_t index)
{
	*mara_stack_addr(ctx, index) = *mara_stack_addr(ctx, -1);
	mara_pop(ctx, 1);
}

mara_index_t
mara_obj_len(mara_context_t* ctx, mara_index_t index);

void
mara_push_gc_obj(mara_context_t* ctx, mara_gc_header_t* gc_obj)
{
	mara_value_set_gc_obj(mara_stack_alloc(ctx), gc_obj);
}
