#include "internal.h"
#include <stdlib.h>
#include "mem_layout.h"

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

	if (options.max_stack <= 0) {
		options.max_stack = 512;
	}

	if (options.max_stackframes <= 0) {
		options.max_stackframes = 512;
	}

	mem_layout_t layout = 0;
	mem_layout_reserve(&layout, sizeof(mara_env_t), _Alignof(mara_env_t));
	ptrdiff_t dummy_chunk_offset = mem_layout_reserve(&layout, sizeof(mara_arena_chunk_t), _Alignof(mara_arena_chunk_t));

	mara_env_t* env = mara_malloc(options.allocator, mem_layout_size(&layout));
	mara_assert(env != NULL, "Out of memory");

	mara_arena_chunk_t* dummy_chunk = mem_layout_locate(env, dummy_chunk_offset);
	dummy_chunk->bump_ptr = dummy_chunk->end = dummy_chunk->begin;
	dummy_chunk->next = NULL;

	*env = (mara_env_t){
		.options = options,
		.permanent_zone = {
			.ref_count = 1,
			.level = -2,
		},
		.dummy_chunk = dummy_chunk,
	};
	mara_arena_init(env, &env->permanent_arena);

	env->permanent_zone.arena = &env->permanent_arena;
	env->permanent_zone.local_snapshot = mara_arena_snapshot(env, &env->permanent_arena);
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
	mara_arena_t control_arena;
	mara_arena_init(env, &control_arena);

	mara_exec_ctx_t* ctx = MARA_ARENA_ALLOC_TYPE(env, &control_arena, mara_exec_ctx_t);
	*ctx = (mara_exec_ctx_t){
		.env = env,
		.error_zone = {
			.ref_count = 1,
			.level = -1,
		},
		.control_arena = control_arena,
		.current_module_options.module_name = mara_str_from_literal("."),
	};
	mara_arena_init(env, &ctx->error_arena);
	mara_arena_init(env, &ctx->debug_info_arena);
	for (mara_index_t i = 0; i < (mara_index_t)MARA_NUM_ARENAS; ++i) {
		mara_arena_init(env, &ctx->arenas[i]);
	}

	ctx->error_zone.arena = &ctx->error_arena;
	ctx->error_zone.local_snapshot = mara_arena_snapshot(env, &ctx->error_arena);
	ctx->zones = mara_arena_alloc_ex(
		env,
		&ctx->control_arena,
		sizeof(mara_zone_t) * env->options.max_stackframes,
		_Alignof(mara_zone_t)
	);
	ctx->stack_frames = mara_arena_alloc_ex(
		env,
		&ctx->control_arena,
		sizeof(mara_stack_frame_t) * env->options.max_stackframes,
		_Alignof(mara_stack_frame_t)
	);
	ctx->stack = mara_arena_alloc_ex(
		env,
		&ctx->control_arena,
		sizeof(mara_value_t) * env->options.max_stack,
		_Alignof(mara_value_t)
	);
	ctx->zone_bookmarks = mara_arena_alloc_ex(
		env,
		&ctx->control_arena,
		sizeof(mara_zone_bookmark_t) * env->options.max_stackframes,
		_Alignof(mara_zone_bookmark_t)
	);
	static mara_zone_options_t initial_options = { 0 };
	mara_zone_enter_new(ctx, &initial_options);
	env->ref_count += 1;
	return ctx;
}

void
mara_end(mara_exec_ctx_t* ctx) {
	while (ctx->current_zone != NULL) {
		mara_zone_exit(ctx);
	}

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
