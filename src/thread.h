#include "internal.h"


mara_thread_t*
mara_alloc_thread(
	mara_context_t* ctx,
	const mara_thread_config_t* config
);

void
mara_gc_mark_thread(
	mara_context_t* ctx,
	mara_thread_t* thread
);

static inline void
mara_release_thread(mara_context_t* ctx, mara_thread_t* thread)
{
	mara_free(ctx, thread);
}
