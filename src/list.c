#include "internal.h"
#include "mara/utils.h"

MARA_PRIVATE mara_error_t*
mara_unbox_list(mara_exec_ctx_t* ctx, mara_value_t value, mara_obj_list_t** result) {
	if (MARA_EXPECT(mara_value_is_list(value))) {
		*result = (mara_obj_list_t*)(mara_value_to_obj(value)->body);
		return NULL;
	} else {
		return mara_type_error(ctx, MARA_VAL_LIST, value);
	}
}

MARA_PRIVATE void
mara_list_free(mara_exec_ctx_t* ctx, void* userdata) {
	mara_free(&ctx->env->options.allocator, ((mara_obj_list_t*)userdata)->elems);
}

MARA_PRIVATE void
mara_list_reserve(
	mara_exec_ctx_t* ctx,
	mara_obj_list_t* obj,
	mara_index_t new_capacity
) {
	mara_assert(new_capacity >= 0, "Invalid capacity");
	mara_assert(new_capacity > obj->capacity, "Unnecessary expand");

	if (obj->in_zone) {
		mara_value_t* new_elems = mara_malloc(
			&ctx->env->options.allocator,
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
			&ctx->env->options.allocator,
			obj->elems,
			sizeof(mara_value_t) * new_capacity
		);
		mara_assert(obj->elems, "Out of memory");
	}

	obj->capacity = new_capacity;
}

mara_value_t
mara_new_list(mara_exec_ctx_t* ctx, mara_zone_t* zone, mara_index_t initial_capacity) {
	mara_obj_t* obj = mara_alloc_obj(
		ctx,
		zone,
		sizeof(mara_obj_list_t)
	);
	obj->type = MARA_OBJ_TYPE_LIST;

	mara_obj_list_t* list = (mara_obj_list_t*)obj->body;
	*list = (mara_obj_list_t){
		.capacity = initial_capacity,
		.len = 0,
		.in_zone = true,
	};
	if (initial_capacity > 0) {
		list->elems = mara_zone_alloc(ctx, zone, sizeof(mara_value_t) * initial_capacity);
	}

	return mara_obj_to_value(obj);
}

mara_error_t*
mara_list_len(mara_exec_ctx_t* ctx, mara_value_t list, mara_index_t* result) {
	mara_obj_list_t* obj;
	mara_check_error(mara_unbox_list(ctx, list, &obj));
	*result = obj->len;
	return NULL;
}

mara_error_t*
mara_list_get(mara_exec_ctx_t* ctx, mara_value_t list, mara_index_t index, mara_value_t* result) {
	mara_obj_list_t* obj;
	mara_check_error(mara_unbox_list(ctx, list, &obj));

	if (MARA_EXPECT(0 <= index && index < obj->len)) {
		*result = obj->elems[index];
		return NULL;
	} else {
		return mara_errorf(
			ctx,
			mara_str_from_literal("core/index-out-of-bound"),
			"List index out of bound: %d",
			mara_value_from_int(index),
			index
		);
	}
}

mara_error_t*
mara_list_set(mara_exec_ctx_t* ctx, mara_value_t list, mara_index_t index, mara_value_t value) {
	mara_obj_list_t* obj;
	mara_check_error(mara_unbox_list(ctx, list, &obj));

	if (MARA_EXPECT(0 <= index && index < obj->len)) {
		mara_obj_t* header = mara_container_of(obj, mara_obj_t, body);
		return mara_copy(ctx, header->zone, value, &obj->elems[index]);
	} else {
		return mara_errorf(
			ctx,
			mara_str_from_literal("core/index-out-of-bound"),
			"List index out of bound: %d",
			mara_value_from_int(index),
			index
		);
	}
}

mara_error_t*
mara_list_push(mara_exec_ctx_t* ctx, mara_value_t list, mara_value_t value) {
	mara_obj_list_t* obj;
	mara_check_error(mara_unbox_list(ctx, list, &obj));

	mara_index_t current_capacity = obj->capacity;
	if (obj->len < current_capacity) {
		mara_obj_t* header = mara_container_of(obj, mara_obj_t, body);
		return mara_copy(ctx, header->zone, value, &obj->elems[obj->len++]);
	} else {
		mara_index_t new_capacity = current_capacity > 0 ? current_capacity * 2 : 4;
		mara_list_reserve(ctx, obj, new_capacity);

		mara_obj_t* header = mara_container_of(obj, mara_obj_t, body);
		return mara_copy(ctx, header->zone, value, &obj->elems[obj->len++]);
	}
}

mara_error_t*
mara_list_delete(mara_exec_ctx_t* ctx, mara_value_t list, mara_index_t index) {
	mara_obj_list_t* obj;
	mara_check_error(mara_unbox_list(ctx, list, &obj));

	if (MARA_EXPECT(0 <= index && index < obj->len)) {
		memmove(
			obj->elems + index,
			obj->elems + index + 1,
			(obj->len - index - 1) * sizeof(mara_value_t)
		);
		obj->len -= 1;
		return NULL;
	} else {
		return mara_errorf(
			ctx,
			mara_str_from_literal("core/index-out-of-bound"),
			"List index out of bound: %d",
			mara_value_from_int(index),
			index
		);
	}
}

mara_error_t*
mara_list_quick_delete(mara_exec_ctx_t* ctx, mara_value_t list, mara_index_t index) {
	mara_obj_list_t* obj;
	mara_check_error(mara_unbox_list(ctx, list, &obj));

	if (MARA_EXPECT(0 <= index && index < obj->len)) {
		obj->elems[index] = obj->elems[obj->len - 1];
		obj->len -= 1;
		return NULL;
	} else {
		return mara_errorf(
			ctx,
			mara_str_from_literal("core/index-out-of-bound"),
			"List index out of bound: %d",
			mara_value_from_int(index),
			index
		);
	}
}

mara_error_t*
mara_list_resize(mara_exec_ctx_t* ctx, mara_value_t list, mara_index_t new_len) {
	mara_obj_list_t* obj;
	mara_check_error(mara_unbox_list(ctx, list, &obj));

	if (new_len < 0) {
		return mara_errorf(
			ctx,
			mara_str_from_literal("core/invalid-arg"),
			"List size is negative: %d",
			mara_value_from_int(new_len),
			new_len
		);
	} else if (new_len <= obj->len) {
		obj->len = new_len;
		return NULL;
	} else if (new_len <= obj->capacity) {
		for (mara_index_t i = obj->len; i < new_len; ++i) {
			obj->elems[i] = mara_null();
		}
		obj->len = new_len;
		return NULL;
	} else {
		mara_list_reserve(ctx, obj, new_len);
		for (mara_index_t i = obj->len; i < new_len; ++i) {
			obj->elems[i] = mara_null();
		}
		obj->len = new_len;
		return NULL;
	}
}
