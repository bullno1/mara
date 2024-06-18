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
	mara_strpool_cleanup(&env->options.allocator, &env->symtab);

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
		.error_zone = {
			.arena = &(mara_arena_t){ 0 },
			.ref_count = 1,
			.branch = MARA_ZONE_BRANCH_ERROR,
		},
	};

	mara_zone_enter_new(&ctx, (mara_zone_options_t){ 0 });
	callback.fn(&ctx, callback.userdata);
	mara_zone_exit(&ctx);

	mara_zone_cleanup(&ctx, &ctx.error_zone);
	mara_arena_reset(env, &ctx.control_arena);
}

void
mara_set_debug_info(mara_exec_ctx_t* ctx, mara_source_info_t debug_info) {
	ctx->current_zone->debug_info = debug_info;
}
