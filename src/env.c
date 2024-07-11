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
		.permanent_zone.level = -1,
		.dummy_chunk = dummy_chunk,
	};
	mara_arena_init(env, &env->permanent_zone.arena);

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
mara_begin(mara_env_t* env, mara_exec_options_t options) {
	if (options.max_stack_size <= 0) {
		options.max_stack_size = 256;
	}

	if (options.max_stack_frames <= 0) {
		options.max_stack_frames = 64;
	}

	mem_layout_t layout = 0;
	mem_layout_reserve(&layout, sizeof(mara_exec_ctx_t), _Alignof(mara_exec_ctx_t));

	ptrdiff_t zones_offset = mem_layout_reserve(
		&layout,
		sizeof(mara_zone_t) * options.max_stack_frames,
		_Alignof(mara_zone_t)
	);
	ptrdiff_t stack_frames_offset = mem_layout_reserve(
		&layout,
		sizeof(mara_stack_frame_t) * options.max_stack_frames,
		_Alignof(mara_stack_frame_t)
	);
	ptrdiff_t debug_info_offset = mem_layout_reserve(
		&layout,
		sizeof(mara_source_info_t*) * options.max_stack_frames,
		_Alignof(mara_source_info_t*)
	);
	ptrdiff_t stack_offset = mem_layout_reserve(
		&layout,
		sizeof(mara_value_t) * options.max_stack_size,
		_Alignof(mara_value_t)
	);

	size_t ctx_size = mem_layout_size(&layout);

	mara_exec_ctx_t* ctx;
	if (env->free_contexts != NULL && env->free_contexts->size >= ctx_size) {
		ctx = env->free_contexts;
		env->free_contexts = ctx->next;
		ctx_size = ctx->size;
	} else {
		ctx = mara_arena_alloc(env, &env->permanent_zone.arena, ctx_size);
	}

	mara_zone_t* current_zone = mem_layout_locate(ctx, zones_offset);
	mara_value_t* stack_base = mem_layout_locate(ctx, stack_offset);
	mara_stack_frame_t* current_stack_frame = mem_layout_locate(ctx, stack_frames_offset);
	*ctx = (mara_exec_ctx_t){
		.env = env,
		.size = ctx_size,
		.current_module_options.module_name = mara_str_from_literal("."),
		.current_zone = current_zone,
		.stack_frames_begin = current_stack_frame,
		.vm_state.fp = current_stack_frame,
		.zones_end = current_zone + options.max_stack_frames,
		.stack_end = stack_base + options.max_stack_size,
		.stack_frames_end = current_stack_frame + options.max_stack_frames,
		.native_debug_info = mem_layout_locate(ctx, debug_info_offset),
		.error_zone = {
			.level = options.max_stack_frames,
		},
	};

	mara_arena_init(env, &ctx->error_zone.arena);
	mara_arena_init(env, &ctx->debug_info_arena);

	*current_zone = (mara_zone_t){ 0 };
	*current_stack_frame = (mara_stack_frame_t){
		.return_zone = current_zone,
		.stack = stack_base,
	};
	ctx->native_debug_info[0] = NULL;
	mara_arena_init(env, &current_zone->arena);

	env->ref_count += 1;
	return ctx;
}

void
mara_end(mara_exec_ctx_t* ctx) {
	mara_env_t* env = ctx->env;

	mara_index_t num_zones = ctx->current_zone->level + 1;
	mara_zone_t* current_zone = ctx->current_zone;
	for (mara_index_t i = 0; i < num_zones; ++i) {
		mara_zone_cleanup(ctx->env, current_zone - i);
	}

	mara_zone_cleanup(env, &ctx->error_zone);
	mara_arena_reset(env, &ctx->debug_info_arena);
	env->ref_count -= 1;

	ctx->next = env->free_contexts;
	env->free_contexts = ctx;
}

bool
mara_reset(mara_env_t* env) {
	if (env->ref_count == 0) {
		mara_symtab_cleanup(env, &env->symtab);
		mara_zone_cleanup(env, &env->permanent_zone);
		env->free_contexts = NULL;
		return true;
	} else {
		return false;
	}
}
