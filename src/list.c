#include "internal.h"
#include <mara/utils.h>

MARA_PRIVATE void
mara_list_free(mara_env_t* env, void* userdata) {
	mara_free(env->options.allocator, ((mara_list_t*)userdata)->elems);
}

MARA_PRIVATE void
mara_list_reserve(
	mara_exec_ctx_t* ctx,
	mara_list_t* obj,
	mara_index_t new_capacity
) {
	mara_assert(new_capacity >= 0, "Invalid capacity");
	mara_assert(new_capacity > obj->capacity, "Unnecessary expand");

	if (obj->in_zone) {
		mara_value_t* new_elems = mara_malloc(
			ctx->env->options.allocator,
			sizeof(mara_value_t) * new_capacity
		);
		mara_assert(new_elems, "Out of memory");
		memcpy(new_elems, obj->elems, sizeof(mara_value_t) * obj->len);
		obj->elems = new_elems;

		mara_obj_t* header = mara_container_of(obj, mara_obj_t, body);
		mara_add_finalizer(ctx, header->zone, (mara_callback_t){
			.fn = mara_list_free,
			.userdata = obj,
		});
		obj->in_zone = false;
	} else {
		obj->elems = mara_realloc(
			ctx->env->options.allocator,
			obj->elems,
			sizeof(mara_value_t) * new_capacity
		);
		mara_assert(obj->elems, "Out of memory");
	}

	obj->capacity = new_capacity;
}

mara_list_t*
mara_new_list(mara_exec_ctx_t* ctx, mara_zone_t* zone, mara_index_t initial_capacity) {
	mara_obj_t* obj = mara_alloc_obj(
		ctx,
		zone,
		sizeof(mara_list_t)
	);
	obj->type = MARA_OBJ_TYPE_LIST;

	mara_list_t* list = (mara_list_t*)obj->body;
	*list = (mara_list_t){
		.capacity = initial_capacity,
		.len = 0,
		.in_zone = true,
	};
	if (initial_capacity > 0) {
		list->elems = mara_zone_alloc_ex(
			ctx, zone,
			sizeof(mara_value_t) * initial_capacity, _Alignof(mara_value_t)
		);
	}

	return list;
}

mara_index_t
mara_list_len(mara_exec_ctx_t* ctx, mara_list_t* list) {
	(void)ctx;
	return list->len;
}

mara_value_t
mara_list_get(mara_exec_ctx_t* ctx, mara_list_t* list, mara_index_t index) {
	(void)ctx;
	if (MARA_EXPECT(0 <= index && index < list->len)) {
		return list->elems[index];
	} else {
		return mara_nil();
	}
}

mara_value_t
mara_list_set(mara_exec_ctx_t* ctx, mara_list_t* list, mara_index_t index, mara_value_t value) {
	if (MARA_EXPECT(0 <= index && index < list->len)) {
		mara_obj_t* header = mara_header_of(list);
		mara_value_t copy = mara_copy(ctx, header->zone, value);
		mara_obj_add_arena_mask(header, copy);
		mara_value_t old_value = list->elems[index];
		list->elems[index] = copy;
		return old_value;
	} else {
		return mara_nil();
	}
}

void
mara_list_push(mara_exec_ctx_t* ctx, mara_list_t* list, mara_value_t value) {
	mara_index_t current_capacity = list->capacity;
	mara_obj_t* header = mara_header_of(list);

	mara_value_t copy = mara_copy(ctx, header->zone, value);

	if (list->len >= current_capacity) {
		mara_index_t new_capacity = current_capacity > 0 ? current_capacity * 2 : 4;
		mara_list_reserve(ctx, list, new_capacity);
	}

	list->elems[list->len++] = copy;
	mara_obj_add_arena_mask(header, copy);
}

mara_value_t
mara_list_delete(mara_exec_ctx_t* ctx, mara_list_t* list, mara_index_t index) {
	(void)ctx;
	if (MARA_EXPECT(0 <= index && index < list->len)) {
		mara_value_t old_value = list->elems[index];
		memmove(
			list->elems + index,
			list->elems + index + 1,
			(list->len - index - 1) * sizeof(mara_value_t)
		);
		list->len -= 1;
		return old_value;
	} else {
		return mara_nil();
	}
}

mara_value_t
mara_list_quick_delete(mara_exec_ctx_t* ctx, mara_list_t* list, mara_index_t index) {
	(void)ctx;
	if (MARA_EXPECT(0 <= index && index < list->len)) {
		mara_value_t old_value = list->elems[index];
		list->elems[index] = list->elems[list->len - 1];
		list->len -= 1;
		return old_value;
	} else {
		return mara_nil();
	}
}

void
mara_list_resize(mara_exec_ctx_t* ctx, mara_list_t* list, mara_index_t new_len) {
	new_len = mara_max(0, new_len);
	if (new_len <= list->len) {
		list->len = new_len;
	} else if (new_len <= list->capacity) {
		for (mara_index_t i = list->len; i < new_len; ++i) {
			list->elems[i] = mara_nil();
		}
		list->len = new_len;
	} else {
		mara_list_reserve(ctx, list, new_len);
		for (mara_index_t i = list->len; i < new_len; ++i) {
			list->elems[i] = mara_nil();
		}
		list->len = new_len;
	}
}

mara_error_t*
mara_list_foreach(mara_exec_ctx_t* ctx, mara_list_t* list, mara_fn_t* fn) {
	mara_index_t len = list->len;
	mara_value_t list_value = mara_value_from_list(list);
	for (mara_index_t i = 0; i < len; ++i) {
		mara_value_t args[] = {
			list->elems[i],
			mara_value_from_int(i),
			list_value,
		};
		mara_value_t should_continue = mara_nil();
		mara_check_error(
			mara_call(ctx, ctx->current_zone, fn, mara_count_of(args), args, &should_continue)
		);

		if (mara_value_is_false(should_continue)) {
			break;
		}
	}

	return NULL;
}
