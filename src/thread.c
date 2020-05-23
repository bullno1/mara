#include "thread.h"
#include "string.h"


mara_thread_t*
mara_alloc_thread(
	mara_context_t* ctx,
	const mara_thread_config_t* config
)
{
	MARA_ASSERT(
		ctx,
		config->stack_size >= sizeof(mara_stack_frame_t)
		&& config->stack_size >= sizeof(mara_value_t),
		"Insufficient stack space"
	);

	mara_thread_t* thread = mara_malloc(
		ctx, sizeof(mara_thread_t) + config->stack_size
	);
	thread->stack_size = config->stack_size;

	if(config->name.length > 0)
	{
		thread->name = mara_alloc_string(ctx, config->name);
		thread->name->gc_header.obj_type = MARA_GC_STRING;
		mara_gc_register(ctx, &thread->name->gc_header);
	}
	else
	{
		thread->name = NULL;
	}

	// Operand stack starts from the end of generic stack space
	const size_t value_alignment = BK_ALIGN_OF(mara_value_t);
	void* stack_end =
		(void*)((uintptr_t)(thread->stack + config->stack_size - 1) / value_alignment * value_alignment);

	// Call stack starts from the beginning of generic stack space
	const size_t stack_frame_alignment = BK_ALIGN_OF(mara_stack_frame_t);
	thread->frame_pointer = thread->frame_pointer_min =
		(void*)((uintptr_t)(thread->stack + stack_frame_alignment - 1) / stack_frame_alignment * stack_frame_alignment);

	// First frame is native
	*thread->frame_pointer = (mara_stack_frame_t) {
		.base_pointer = stack_end,
		.stack_pointer = stack_end,
		.is_native = true,
	};

	return thread;
}

void
mara_gc_mark_thread(mara_context_t* ctx, mara_thread_t* thread)
{
	mara_gc_mark(ctx, &thread->name->gc_header);
	for(
		mara_stack_frame_t* stack_frame = thread->frame_pointer_min;
		stack_frame <= thread->frame_pointer;
		++stack_frame
	)
	{
		// TODO: Mark closure
		for(
			mara_value_t* value = stack_frame->stack_pointer + 1;
			value <= stack_frame->base_pointer ;
			++value
		)
		{
			if(mara_value_type_check(value, MARA_VAL_GC_OBJ))
			{
				mara_gc_mark(ctx, mara_value_as_gc_obj(value));
			}
		}
	}
}
