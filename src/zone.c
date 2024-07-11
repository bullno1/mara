#include "internal.h"

void
mara_zone_cleanup(mara_env_t* env, mara_zone_t* zone) {
	if (zone->arena.current_chunk != NULL) {
		for (
			mara_finalizer_t* itr = zone->finalizers;
			itr != NULL;
			itr = itr->next
		) {
			itr->callback.fn(env, itr->callback.userdata);
		}

		mara_arena_reset(env, &zone->arena);
	}
}

mara_zone_t*
mara_zone_enter(mara_exec_ctx_t* ctx) {
	mara_zone_t* current_zone = ctx->current_zone;
	mara_zone_t* new_zone = ++ctx->current_zone;
	if (MARA_EXPECT(new_zone < ctx->zones_end)) {
		new_zone->level = current_zone->level + 1;
		new_zone->finalizers = NULL;
		new_zone->arena.current_chunk = NULL;

		if (ctx->last_error.type.len) {
			mara_zone_cleanup(ctx->env, &ctx->error_zone);
		}

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
mara_add_finalizer(mara_exec_ctx_t* ctx, mara_zone_t* zone, mara_callback_t callback) {
	mara_finalizer_t* finalizer = MARA_ZONE_ALLOC_TYPE(ctx, zone, mara_finalizer_t);
	mara_assert(finalizer != NULL, "Out of memory");

	finalizer->callback = callback;
	finalizer->next = zone->finalizers;
	zone->finalizers = finalizer;
}

void*
mara_zone_alloc(mara_exec_ctx_t* ctx, mara_zone_t* zone, size_t size) {
	return mara_arena_alloc(ctx->env, &zone->arena, size);
}

void*
mara_zone_alloc_ex(mara_exec_ctx_t* ctx, mara_zone_t* zone, size_t size, size_t alignment) {
	return mara_arena_alloc_ex(ctx->env, &zone->arena, size, alignment);
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
	return (mara_zone_snapshot_t){
		.arena_snapshot = mara_arena_snapshot(ctx->env, &zone->arena),
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
	mara_arena_restore(ctx->env, &zone->arena, snapshot.arena_snapshot);
}
