#include "internal.h"
#include <string.h>

MARA_PRIVATE mara_zone_t*
mara_zone_new(mara_exec_ctx_t* ctx, mara_zone_options_t options) {
	mara_zone_t* current_zone = ctx->current_zone;

	mara_arena_snapshot_t control_snapshot = mara_arena_snapshot(ctx, &ctx->control_arena);
	mara_zone_t* new_zone = mara_arena_alloc(ctx, &ctx->control_arena, sizeof(mara_zone_t));
	mara_assert(new_zone != NULL, "Out of memory");

	mara_arena_t* arena_for_zone = NULL;
	mara_arena_t* ctx_arenas = ctx->arenas;
	{
		for (mara_arena_t* itr = ctx_arenas; itr != NULL; itr = itr->next) {
			itr->in_use = false;
		}

		if (current_zone != NULL) {
			current_zone->arena->in_use = true;
		}

		for (mara_index_t i = 0; i < options.argc; ++i) {
			mara_obj_t* obj = mara_value_to_obj(options.argv[i]);
			if (obj == NULL) { continue; }

			obj->zone->arena->in_use = true;
		}

		for (mara_index_t i = 0; i < options.num_marked_zones; ++i) {
			options.marked_zones[i]->arena->in_use = true;
		}

		for (mara_arena_t* itr = ctx_arenas; itr != NULL; itr = itr->next) {
			if (!itr->in_use) {
				arena_for_zone = itr;
				break;
			}
		}

		if (arena_for_zone == NULL) {
			mara_arena_t* arena = mara_arena_alloc(ctx, &ctx->control_arena, sizeof(mara_arena_t));
			*arena = (mara_arena_t){
				.next = ctx_arenas,
			};
			ctx->arenas = arena;
			arena_for_zone = arena;
		}
	}

	*new_zone = (mara_zone_t){
		.level = current_zone != NULL ? current_zone->level + 1 : 0,
		.branch = current_zone != NULL ? current_zone->branch : MARA_ZONE_BRANCH_MAIN,
		.arena = arena_for_zone,
		.ctx_arenas = ctx_arenas,
		.control_snapshot = control_snapshot,
		.local_snapshot = mara_arena_snapshot(ctx, arena_for_zone),
	};

	return new_zone;
}

void
mara_zone_enter_new(mara_exec_ctx_t* ctx, mara_zone_options_t options) {
	mara_zone_enter(ctx, mara_zone_new(ctx, options));
}

void
mara_zone_enter(mara_exec_ctx_t* ctx, mara_zone_t* zone) {
	mara_arena_snapshot_t control_snapshot = mara_arena_snapshot(
		ctx, &ctx->control_arena
	);
	mara_zone_bookmark_t* bookmark = mara_arena_alloc(
		ctx, &ctx->control_arena, sizeof(mara_zone_bookmark_t)
	);
	bookmark->previous_bookmark = ctx->current_zone_bookmark;
	bookmark->previous_zone = ctx->current_zone;
	bookmark->control_snapshot = control_snapshot;

	zone->ref_count += 1;

	ctx->current_zone_bookmark = bookmark;
	ctx->current_zone = zone;

	// Reset error
	ctx->last_error = (mara_error_t){ 0 };
	mara_zone_cleanup(ctx, &ctx->error_zone);
}

void
mara_zone_exit(mara_exec_ctx_t* ctx) {
	mara_zone_t* current_zone = ctx->current_zone;

	mara_zone_bookmark_t* bookmark = ctx->current_zone_bookmark;
	ctx->current_zone = bookmark->previous_zone;
	ctx->current_zone_bookmark = bookmark->previous_bookmark;
	mara_arena_restore(ctx, &ctx->control_arena, bookmark->control_snapshot);

	if (--current_zone->ref_count == 0) {
		mara_zone_cleanup(ctx, current_zone);
		ctx->arenas = current_zone->ctx_arenas;
		mara_arena_restore(ctx, &ctx->control_arena, current_zone->control_snapshot);
	}
}

mara_zone_t*
mara_zone_switch(mara_exec_ctx_t* ctx, mara_zone_t* zone) {
	mara_zone_t* old_zone = ctx->current_zone;
	ctx->current_zone = zone;
	return old_zone;
}

void
mara_zone_cleanup(mara_exec_ctx_t* ctx, mara_zone_t* zone) {
	for (
		mara_finalizer_t* itr = zone->finalizers;
		itr != NULL;
		itr = itr->next
	) {
		itr->callback.fn(ctx, itr->callback.userdata);
	}

	mara_arena_restore(ctx, zone->arena, zone->local_snapshot);
}

void
mara_add_finalizer(mara_exec_ctx_t* ctx, mara_zone_t* zone, mara_callback_t callback) {
	mara_finalizer_t* finalizer = mara_zone_alloc(ctx, zone, sizeof(mara_finalizer_t));
	mara_assert(finalizer != NULL, "Out of memory");

	finalizer->callback = callback;
	finalizer->next = zone->finalizers;
	zone->finalizers = finalizer;
}

void
mara_defer(mara_exec_ctx_t* ctx, mara_callback_t callback) {
	mara_add_finalizer(ctx, ctx->current_zone, callback);
}

void*
mara_zone_alloc(mara_exec_ctx_t* ctx, mara_zone_t* zone, size_t size) {
	return mara_arena_alloc(ctx, zone->arena, size);
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
