#include "internal.h"
#include <stdlib.h>

MARA_PRIVATE void*
mara_libc_alloc(void* ptr, size_t new_size, void* userdata) {
	(void)userdata;
	if (new_size == 0) {
		free(ptr);
		return NULL;
	} else {
		return realloc(ptr, new_size);
	}
}

mara_env_t*
mara_create_env(mara_env_options_t options) {
	if (options.allocator.fn == NULL) {
		options.allocator.fn = mara_libc_alloc;
	}

	if (options.alloc_chunk_size < sizeof(mara_arena_chunk_t)) {
		options.alloc_chunk_size = 4096 * 4;
	}

	mara_env_t* env = mara_malloc(&options.allocator, sizeof(mara_env_t));
	mara_assert(env != NULL, "Out of memory");

	*env = (mara_env_t){
		.options = options,
	};

	return env;
}

void
mara_destroy_env(mara_env_t* env) {
	mara_allocator_t* allocator = &env->options.allocator;
	for (mara_arena_chunk_t* itr = env->free_chunks; itr != NULL;) {
		mara_arena_chunk_t* next = itr->next;
		mara_free(allocator, itr);
		itr = next;
	}

	mara_free(&env->options.allocator, env);
}

void
mara_exec(mara_env_t* env, mara_callback_t callback) {
	mara_exec_ctx_t ctx = {
		.env = env,
		.context_zone = {
			.arena = &(mara_arena_t){ 0 },
		},
	};

	mara_zone_enter(&ctx, 0, NULL);
	callback.fn(&ctx, callback.userdata);
	mara_zone_exit(&ctx);

	mara_zone_cleanup(&ctx, &ctx.context_zone);
	mara_arena_restore(&ctx, &ctx.control_arena, (mara_arena_snapshot_t) { 0 });
}
