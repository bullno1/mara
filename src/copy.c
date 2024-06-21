#include "internal.h"
#include "xxhash.h"

#define BHAMT_IS_TOMBSTONE(node) false
#define BHAMT_KEYEQ(lhs, rhs) ((lhs) == (rhs))

typedef struct mara_ptr_map_node_s {
	void* key;
	void* value;
	struct mara_ptr_map_node_s* children[BHAMT_NUM_CHILDREN];
} mara_ptr_map_node_t;

typedef struct {
	mara_ptr_map_node_t* root;
} mara_ptr_map_t;

MARA_PRIVATE void
mara_ptr_map_put(
	mara_exec_ctx_t* ctx,
	mara_zone_t* zone,
	mara_ptr_map_t* map,
	void* key,
	void* value
) {
	BHAMT_HASH_TYPE hash = mara_XXH3_64bits(&key, sizeof(key));
	mara_ptr_map_node_t** itr;
	mara_ptr_map_node_t* result;
	mara_ptr_map_node_t* free_node;
	(void)free_node;
	BHAMT_SEARCH(map->root, itr, result, free_node, hash, key);

	if (result == NULL) {
		result = *itr = MARA_ZONE_ALLOC_TYPE(ctx, zone, mara_ptr_map_node_t);
		memset(result->children, 0, sizeof(result->children));
		result->key = key;
		result->value = value;
	}
}

MARA_PRIVATE void*
mara_ptr_map_get(mara_ptr_map_t* map, void* key) {
	BHAMT_HASH_TYPE hash = mara_XXH3_64bits(&key, sizeof(key));
	mara_ptr_map_node_t* result;
	BHAMT_GET(map->root, result, hash, key);
	if (result != NULL) {
		return result->value;
	} else {
		return NULL;
	}
}

MARA_PRIVATE mara_error_t*
mara_deep_copy(
	mara_exec_ctx_t* ctx,
	mara_zone_t* target_zone,
	mara_ptr_map_t* copied_objs,
	mara_value_t value,
	mara_value_t* result
) {
	mara_obj_t* obj = mara_value_to_obj(value);

	// TODO: rethink the concept of branch
	if (
		obj == NULL
		|| (
			obj->zone->branch == target_zone->branch
			&& obj->zone->level <= target_zone->level
		)
	) {
		*result = value;
		return NULL;
	}

	mara_obj_t* copied_obj = mara_ptr_map_get(copied_objs, obj);
	if (copied_obj != NULL) {
		*result = mara_obj_to_value(copied_obj);
		return NULL;
	}

	mara_zone_t* local_zone = mara_get_local_zone(ctx);
	switch (obj->type) {
		case MARA_OBJ_TYPE_STRING:
			{
				mara_str_t* str = (mara_str_t*)obj->body;
				*result = mara_new_str(ctx, target_zone, *str);
				mara_ptr_map_put(ctx, local_zone, copied_objs, obj, mara_value_to_obj(*result));
				return NULL;
			}
		case MARA_OBJ_TYPE_REF:
			{
				mara_obj_ref_t* ref = (mara_obj_ref_t*)obj->body;
				*result = mara_new_ref(ctx, target_zone, ref->tag, ref->value);
				mara_ptr_map_put(ctx, local_zone, copied_objs, obj, mara_value_to_obj(*result));
				return NULL;
			}
		case MARA_OBJ_TYPE_NATIVE_FN:
			{
				mara_native_fn_t* fn = (mara_native_fn_t*)obj->body;
				*result = mara_new_fn(ctx, target_zone, *fn);
				mara_ptr_map_put(ctx, local_zone, copied_objs, obj, mara_value_to_obj(*result));
				return NULL;
			}
		case MARA_OBJ_TYPE_LIST:
			{
				mara_obj_list_t* old_list;
				mara_assert_no_error(mara_unbox_list(ctx, value, &old_list));

				mara_value_t new_list = mara_new_list(ctx, target_zone, old_list->len);
				mara_ptr_map_put(ctx, local_zone, copied_objs, obj, mara_value_to_obj(new_list));
				mara_obj_list_t* new_list_obj;
				mara_assert_no_error(mara_unbox_list(ctx, new_list, &new_list_obj));

				mara_index_t len = new_list_obj->len = old_list->len;
				mara_value_t* old_elems = old_list->elems;
				mara_value_t* new_elems = new_list_obj->elems;
				mara_obj_t* new_list_header = mara_value_to_obj(new_list);
				for (mara_index_t i = 0; i < len; ++i) {
					mara_value_t elem_copy;
					mara_check_error(mara_deep_copy(ctx, target_zone, copied_objs, old_elems[i], &elem_copy));

					new_elems[i] = elem_copy;
					mara_obj_add_arena_mask(new_list_header, elem_copy);
				}

				*result = new_list;
				return NULL;
			}
		case MARA_OBJ_TYPE_MAP:
			{
				mara_obj_map_t* old_map;
				mara_assert_no_error(mara_unbox_map(ctx, value, &old_map));

				mara_value_t new_map = mara_new_map(ctx, target_zone);
				mara_ptr_map_put(ctx, local_zone, copied_objs, obj, mara_value_to_obj(new_map));

				for (
					mara_obj_map_node_t* itr = old_map->root;
					itr != NULL;
					itr = itr->next
				) {
					mara_value_t key_copy, value_copy;
					mara_check_error(mara_deep_copy(ctx, target_zone, copied_objs, itr->key, &key_copy));
					mara_check_error(mara_deep_copy(ctx, target_zone, copied_objs, itr->value, &value_copy));
					mara_assert_no_error(mara_map_set(ctx, new_map, key_copy, value_copy));
				}

				*result = new_map;
				return NULL;
			}
		case MARA_OBJ_TYPE_MARA_FN:
			{
				mara_obj_closure_t* old_closure = (mara_obj_closure_t*)obj->body;
				mara_index_t num_captures = old_closure->fn->num_captures;
				mara_obj_t* new_closure_header = mara_alloc_obj(
					ctx, target_zone,
					sizeof(mara_obj_closure_t) + sizeof(mara_value_t) * num_captures
				);
				new_closure_header->type = MARA_OBJ_TYPE_MARA_FN;
				mara_ptr_map_put(ctx, local_zone, copied_objs, obj, new_closure_header);

				mara_obj_closure_t* new_closure = (mara_obj_closure_t*)new_closure_header->body;
				new_closure->fn = old_closure->fn;
				for (mara_index_t i = 0; i < num_captures; ++i) {
					mara_value_t capture_copy;
					mara_check_error(
						mara_deep_copy(
							ctx, target_zone, copied_objs,
							old_closure->captures[i], &capture_copy
						)
					);
					new_closure->captures[i] = capture_copy;
					mara_obj_add_arena_mask(new_closure_header, capture_copy);
				}

				*result = mara_obj_to_value(new_closure_header);
				return NULL;
			}
		default:
			return mara_errorf(
				ctx,
				mara_str_from_literal("core/panic"),
				"Invalid object type: %d",
				mara_value_from_int(obj->type),
				obj->type
			);
	}
}

MARA_PRIVATE mara_error_t*
mara_start_deep_copy(mara_exec_ctx_t* ctx, mara_zone_t* zone, mara_value_t value, mara_value_t* result) {
	// TODO: Adjust copy policy to just ban it.
	// Copy must be made explicitly.
	// Encourage constructing objects in the correct zone in the first place.

	// value is not included because we are not modifying it
	mara_zone_enter_new(ctx, (mara_zone_options_t){
		.num_marked_zones = 1,
		.marked_zones = (mara_zone_t*[]){ zone },
	});
	mara_ptr_map_t copied_objs = { .root = NULL };
	mara_error_t* error = mara_deep_copy(ctx, zone, &copied_objs, value, result);
	mara_zone_exit(ctx);
	return error;
}

mara_error_t*
mara_copy(mara_exec_ctx_t* ctx, mara_zone_t* zone, mara_value_t value, mara_value_t* result) {
	mara_obj_t* obj = mara_value_to_obj(value);
	if (
		obj == NULL
		|| (
			obj->zone->branch == zone->branch
			&& obj->zone->level <= zone->level
		)
	) {
		*result = value;
		return NULL;
	}

	switch (obj->type) {
		case MARA_OBJ_TYPE_STRING:
			{
				mara_str_t* str = (mara_str_t*)obj->body;
				*result = mara_new_str(ctx, zone, *str);
				return NULL;
			}
		case MARA_OBJ_TYPE_REF:
			{
				mara_obj_ref_t* ref = (mara_obj_ref_t*)obj->body;
				*result = mara_new_ref(ctx, zone, ref->tag, ref->value);
				return NULL;
			}
		case MARA_OBJ_TYPE_NATIVE_FN:
			{
				mara_native_fn_t* fn = (mara_native_fn_t*)obj->body;
				*result = mara_new_fn(ctx, zone, *fn);
				return NULL;
			}
		case MARA_OBJ_TYPE_LIST:
		case MARA_OBJ_TYPE_MAP:
		case MARA_OBJ_TYPE_MARA_FN:
			return mara_start_deep_copy(ctx, zone, value, result);
		default:
			return mara_errorf(
				ctx,
				mara_str_from_literal("core/panic"),
				"Invalid object type: %d",
				mara_value_from_int(obj->type),
				obj->type
			);
	}
}
