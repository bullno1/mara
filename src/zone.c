#include "internal.h"

#if defined(__GNUC__)

MARA_PRIVATE uint32_t
mara_ffs(uint32_t mask) {
	return __builtin_ffs(mask) - 1;
}

#elif defined(_MSC_VER)

#include <intrin.h>

MARA_PRIVATE uint32_t
mara_ffs(uint32_t mask) {
    unsigned long i;
    _BitScanForward(&i, mask);
	return i;
}

#else
#error "Unsupported platform"
#endif

MARA_PRIVATE mara_zone_t*
mara_zone_new(mara_exec_ctx_t* ctx, mara_zone_options_t* options) {
	mara_zone_t* current_zone = ctx->current_zone;

	mara_index_t zone_index = ctx->num_zones++;
	mara_zone_t* new_zone = &ctx->zones[zone_index];
	mara_assert(new_zone != NULL, "Out of memory");

	*new_zone = (mara_zone_t){
		// TODO: level might just be index
		.level = current_zone != NULL ? current_zone->level + 1 : 0,
		.index = zone_index,
		// TODO: combine parent, level, index
		.parent = current_zone,
		.options = options,
	};

	return new_zone;
}

void
mara_zone_enter_new(mara_exec_ctx_t* ctx, mara_zone_options_t* options) {
	mara_zone_enter(ctx, mara_zone_new(ctx, options));
}

void
mara_zone_enter(mara_exec_ctx_t* ctx, mara_zone_t* zone) {
	mara_zone_bookmark_t* bookmark = ctx->zone_bookmarks;
	ctx->zone_bookmarks++;
	bookmark->previous_bookmark = ctx->current_zone_bookmark;
	bookmark->previous_zone = ctx->current_zone;

	zone->ref_count += 1;

	ctx->current_zone_bookmark = bookmark;
	ctx->current_zone = zone;

	// Reset error
	mara_zone_cleanup(ctx->env, &ctx->error_zone);
}

void
mara_zone_exit(mara_exec_ctx_t* ctx) {
	mara_zone_t* current_zone = ctx->current_zone;

	mara_zone_bookmark_t* bookmark = ctx->current_zone_bookmark;
	ctx->current_zone = bookmark->previous_zone;
	ctx->current_zone_bookmark = bookmark->previous_bookmark;
	ctx->zone_bookmarks = bookmark;

	if (--current_zone->ref_count == 0) {
		mara_zone_cleanup(ctx->env, current_zone);
		ctx->num_zones = current_zone->index;
	}
}

mara_zone_t*
mara_zone_switch(mara_exec_ctx_t* ctx, mara_zone_t* zone) {
	mara_zone_t* old_zone = ctx->current_zone;
	ctx->current_zone = zone;
	return old_zone;
}

void
mara_zone_cleanup(mara_env_t* env, mara_zone_t* zone) {
	for (
		mara_finalizer_t* itr = zone->finalizers;
		itr != NULL;
		itr = itr->next
	) {
		itr->callback.fn(env, itr->callback.userdata);
	}

	if (zone->arena != NULL) {
		mara_arena_restore(env, zone->arena, zone->local_snapshot);
	}
}

void
mara_add_finalizer(mara_exec_ctx_t* ctx, mara_zone_t* zone, mara_callback_t callback) {
	mara_finalizer_t* finalizer = MARA_ZONE_ALLOC_TYPE(ctx, zone, mara_finalizer_t);
	mara_assert(finalizer != NULL, "Out of memory");

	finalizer->callback = callback;
	finalizer->next = zone->finalizers;
	zone->finalizers = finalizer;
}

void*
mara_zone_alloc(mara_exec_ctx_t* ctx, mara_zone_t* zone, size_t size) {
	mara_arena_t* arena = mara_get_zone_arena(ctx, zone);
	return mara_arena_alloc(ctx->env, arena, size);
}

void*
mara_zone_alloc_ex(mara_exec_ctx_t* ctx, mara_zone_t* zone, size_t size, size_t alignment) {
	mara_arena_t* arena = mara_get_zone_arena(ctx, zone);
	return mara_arena_alloc_ex(ctx->env, arena, size, alignment);
}

mara_zone_t*
mara_get_local_zone(mara_exec_ctx_t* ctx) {
	return ctx->current_zone;
}

mara_zone_t*
mara_get_return_zone(mara_exec_ctx_t* ctx) {
	mara_zone_bookmark_t* bookmark = ctx->current_zone_bookmark;
	return bookmark->previous_zone != NULL ? bookmark->previous_zone : ctx->current_zone;
}

mara_zone_t*
mara_get_error_zone(mara_exec_ctx_t* ctx) {
	return &ctx->error_zone;
}

mara_zone_t*
mara_get_zone_of(mara_exec_ctx_t* ctx, mara_value_t value) {
	mara_obj_t* obj = mara_value_to_obj(value);
	return obj != NULL ? obj->zone : mara_get_local_zone(ctx);
}

mara_arena_mask_t
mara_arena_mask_of_zone(mara_exec_ctx_t* ctx, mara_zone_t* zone) {
	mara_arena_t* zone_arena = mara_get_zone_arena(ctx, zone);
	if (
		MARA_EXPECT(
			zone->level >= 0
			&& ctx->arenas <= zone_arena
			&& zone_arena < (ctx->arenas + MARA_NUM_ARENAS)
		)
	) {
		return 1 << (zone_arena - ctx->arenas);
	} else {
		return 0;
	}
}

mara_zone_snapshot_t
mara_zone_snapshot(mara_exec_ctx_t* ctx) {
	mara_zone_t* zone = mara_get_local_zone(ctx);
	mara_arena_t* zone_arena = mara_get_zone_arena(ctx, zone);
	return (mara_zone_snapshot_t){
		.arena_snapshot = mara_arena_snapshot(ctx->env, zone_arena),
		.finalizers = zone->finalizers,
	};
}

void
mara_zone_restore(mara_exec_ctx_t* ctx, mara_zone_snapshot_t snapshot) {
	mara_zone_t* zone = mara_get_local_zone(ctx);
	mara_finalizer_t* finalizer = zone->finalizers;
	while (finalizer != snapshot.finalizers) {
		mara_finalizer_t* next = finalizer->next;
		finalizer->callback.fn(ctx->env, finalizer->callback.userdata);
		finalizer = next;
	}

	zone->finalizers = snapshot.finalizers;
	mara_arena_restore(ctx->env, zone->arena, snapshot.arena_snapshot);
}

mara_arena_t*
mara_get_zone_arena(mara_exec_ctx_t* ctx, mara_zone_t* zone) {
	if (zone->arena != NULL) {
		return zone->arena;
	} else {
		mara_zone_t* current_zone = zone->parent;
		mara_zone_options_t* options = zone->options;
		// Find an arena for this new zone.
		// It cannot be used by:
		//
		// * The previous zone
		// * Any marked zones
		// * Any objects passed as arguments
		mara_arena_t* arena_for_zone = NULL;
		{
			mara_arena_mask_t arena_mask = 0;

			if (current_zone != NULL) {
				arena_mask |= mara_arena_mask_of_zone(ctx, current_zone);
			}

			for (mara_index_t i = 0; i < options->argc; ++i) {
				mara_obj_t* obj = mara_value_to_obj(options->argv[i]);
				if (obj == NULL) { continue; }

				arena_mask |= obj->arena_mask;
			}

			// Module system objects are implicitly passed to all zones
			if (ctx->module_loaders != NULL) {
				arena_mask |= mara_header_of(ctx->module_loaders)->arena_mask;
			}
			if (ctx->current_module != NULL) {
				arena_mask |= mara_header_of(ctx->current_module)->arena_mask;
			}

			for (mara_index_t i = 0; i < options->num_marked_zones; ++i) {
				arena_mask |= mara_arena_mask_of_zone(ctx, options->marked_zones[i]);
			}

			mara_arena_mask_t free_mask = ~arena_mask;
			if (MARA_EXPECT(free_mask != 0)) {
				mara_index_t arena_index = mara_ffs(free_mask);
				mara_assert(arena_index < (mara_index_t)MARA_NUM_ARENAS, "Index out of bound");
				arena_for_zone = &ctx->arenas[arena_index];
			} else {
				// Alloc a new arena with its metadata in the control arena.
				// This will be automatically cleaned up when this zone exit due
				// to control_snapshot.
				// The problem is that it grabs a brand new chunk which increases
				// peak memory usage.
				mara_arena_t* arena = MARA_ARENA_ALLOC_TYPE(
					ctx->env, &ctx->control_arena, mara_arena_t
				);
				*arena = (mara_arena_t){ 0 };
				arena_for_zone = arena;
			}
		}
		zone->arena = arena_for_zone;
		zone->local_snapshot = mara_arena_snapshot(ctx->env, arena_for_zone);
		return zone->arena;
	}
}
