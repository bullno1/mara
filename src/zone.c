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

MARA_PRIVATE void
mara_free_arena(mara_env_t* env, void* userdata) {
	mara_free(env->options.allocator, userdata);
}

mara_zone_t*
mara_zone_enter(mara_exec_ctx_t* ctx, mara_zone_options_t options) {
	mara_zone_t* current_zone = ctx->current_zone;
	mara_zone_t* new_zone = ++ctx->current_zone;
	if (MARA_EXPECT(new_zone < ctx->zones_end)) {
		*new_zone = (mara_zone_t){
			.level = current_zone->level + 1,
			.options = options,
		};

		mara_zone_cleanup(ctx->env, &ctx->error_zone);
		return new_zone;
	} else {
		return NULL;
	}
}

void
mara_zone_exit(mara_exec_ctx_t* ctx, mara_zone_t* zone) {
	mara_assert(zone == ctx->current_zone, "Unmatched zone");
	mara_assert(zone->level > 0, "Illegal zone exit");
	mara_zone_cleanup(ctx->env, zone);
	--ctx->current_zone;
}

void
mara_zone_cleanup(mara_env_t* env, mara_zone_t* zone) {
	if (zone->arena != NULL) {
		// In case the arena is allocated on the heap, release the chunks first.
		// This should be safe as finalizers deal with unmanaged heap memory.
		mara_arena_restore(env, zone->arena, zone->arena_snapshot);

		for (
			mara_finalizer_t* itr = zone->finalizers;
			itr != NULL;
			itr = itr->next
		) {
			itr->callback.fn(env, itr->callback.userdata);
		}
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
	mara_arena_t* arena = mara_zone_get_arena(ctx, zone);
	return mara_arena_alloc(ctx->env, arena, size);
}

void*
mara_zone_alloc_ex(mara_exec_ctx_t* ctx, mara_zone_t* zone, size_t size, size_t alignment) {
	mara_arena_t* arena = mara_zone_get_arena(ctx, zone);
	return mara_arena_alloc_ex(ctx->env, arena, size, alignment);
}

mara_zone_t*
mara_get_local_zone(mara_exec_ctx_t* ctx) {
	return ctx->current_zone;
}

mara_zone_t*
mara_get_return_zone(mara_exec_ctx_t* ctx) {
	return ctx->vm_state.fp->return_zone;
}

mara_zone_t*
mara_get_error_zone(mara_exec_ctx_t* ctx) {
	return &ctx->error_zone;
}

mara_zone_snapshot_t
mara_zone_snapshot(mara_exec_ctx_t* ctx) {
	mara_zone_t* zone = mara_get_local_zone(ctx);
	mara_arena_t* zone_arena = mara_zone_get_arena(ctx, zone);
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
mara_zone_get_arena(mara_exec_ctx_t* ctx, mara_zone_t* zone) {
	if (zone->arena != NULL) {
		return zone->arena;
	} else {
		const mara_zone_options_t* options = &zone->options;
		// Find an arena for this new zone.
		// It cannot be used by:
		//
		// * The return zone
		// * Any objects passed as arguments
		// * The closure being called
		mara_arena_mask_t used_arena_mask = 0;

		// Module system objects are implicitly passed to all zones
		if (ctx->module_loaders != NULL) {
			used_arena_mask |= mara_header_of(ctx->module_loaders)->arena_mask;
		}
		if (ctx->current_module != NULL) {
			used_arena_mask |= mara_header_of(ctx->current_module)->arena_mask;
		}

		for (mara_index_t i = 0; i < options->argc; ++i) {
			mara_value_t arg = options->argv[i];
			if (mara_value_is_obj(arg)) {
				mara_obj_t* obj = mara_value_to_obj(arg);
				used_arena_mask |= obj->arena_mask;
			}
		}

		mara_zone_t* return_zone = options->return_zone;
		if (return_zone != NULL) {
			// Ensure that the return zone has an arena mask
			mara_zone_get_arena(ctx, return_zone);
			used_arena_mask |= return_zone->arena_mask;
		}

		if (options->vm_closure != NULL) {
			used_arena_mask |= mara_header_of(options->vm_closure)->arena_mask;
		}

		mara_arena_mask_t free_mask = ~used_arena_mask;
		mara_arena_t* assigned_arena;
		mara_arena_mask_t assigned_arena_mask;
		if (MARA_EXPECT(free_mask != 0)) {
			mara_index_t arena_index = mara_ffs(free_mask);
			mara_assert(arena_index < (mara_index_t)MARA_NUM_ARENAS, "Index out of bound");
			assigned_arena = &ctx->arenas[arena_index];
			assigned_arena_mask = 1 << arena_index;
		} else {
			// This is unlikely to happen.
			// Alloc a new arena.
			assigned_arena = mara_malloc(ctx->env->options.allocator, sizeof(mara_arena_t));
			assigned_arena_mask = 0;
			mara_arena_init(ctx->env, assigned_arena);
			mara_add_finalizer(ctx, zone, (mara_callback_t){
				.fn = mara_free_arena,
				.userdata = assigned_arena,
			});
		}

		zone->arena = assigned_arena;
		zone->arena_mask = assigned_arena_mask;
		zone->arena_snapshot = mara_arena_snapshot(ctx->env, assigned_arena);
		return assigned_arena;
	}
}
