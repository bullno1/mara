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

	mara_env_t* env = mara_malloc(options.allocator, sizeof(mara_env_t));
	mara_assert(env != NULL, "Out of memory");

	*env = (mara_env_t){
		.options = options,
		.permanent_zone = {
			.ref_count = 1,
			.level = -2,
		},
		.module_cache = mara_nil(),
	};
	env->permanent_zone.arena = &env->permanent_arena;
	mara_symtab_init(env, &env->symtab);

	return env;
}

void
mara_destroy_env(mara_env_t* env) {
	mara_assert(mara_reset(env), "env is still in use");

	mara_allocator_t allocator = env->options.allocator;
	for (mara_arena_chunk_t* itr = env->free_chunks; itr != NULL;) {
		mara_arena_chunk_t* next = itr->next;
		mara_free(allocator, itr);
		itr = next;
	}

	mara_free(allocator, env);
}

mara_exec_ctx_t*
mara_begin(mara_env_t* env) {
	mara_arena_t control_arena = { 0 };
	mara_exec_ctx_t* ctx = MARA_ARENA_ALLOC_TYPE(env, &control_arena, mara_exec_ctx_t);
	*ctx = (mara_exec_ctx_t){
		.env = env,
		.error_zone = {
			.ref_count = 1,
			.level = -1,
		},
		.control_arena = control_arena,
		.current_module = mara_nil(),
		.module_loaders = mara_nil(),
	};
	ctx->error_zone.arena = &ctx->error_arena;
	mara_zone_enter_new(ctx, (mara_zone_options_t){ 0 });
	env->ref_count += 1;
	return ctx;
}

void
mara_end(mara_exec_ctx_t* ctx) {
	mara_zone_exit(ctx);

	mara_env_t* env = ctx->env;
	mara_zone_cleanup(env, &ctx->error_zone);
	mara_arena_reset(env, &ctx->control_arena);
	mara_arena_reset(env, &ctx->debug_info_arena);
	env->ref_count -= 1;
}

bool
mara_reset(mara_env_t* env) {
	if (env->ref_count == 0) {
		mara_symtab_cleanup(env, &env->symtab);
		mara_zone_cleanup(env, &env->permanent_zone);
		return true;
	} else {
		return false;
	}
}
