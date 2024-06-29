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

MARA_PRIVATE mara_value_t
mara_deep_copy(
	mara_exec_ctx_t* ctx,
	mara_zone_t* target_zone,
	mara_ptr_map_t* copied_objs,
	mara_value_t value
) {
	if (!mara_value_is_obj(value)) {
		return value;
	}

	mara_obj_t* obj = mara_value_to_obj(value);
	if (obj->zone->level <= target_zone->level) {
		return value;
	}

	mara_obj_t* copied_obj = mara_ptr_map_get(copied_objs, obj);
	if (copied_obj != NULL) {
		return mara_obj_to_value(copied_obj);
	}

	mara_zone_t* local_zone = mara_get_local_zone(ctx);
	switch (obj->type) {
		case MARA_OBJ_TYPE_STRING:
			{
				mara_str_t* str = (mara_str_t*)obj->body;
				mara_value_t result = mara_new_str(ctx, target_zone, *str);
				mara_ptr_map_put(ctx, local_zone, copied_objs, obj, mara_value_to_obj(result));
				return result;
			}
		case MARA_OBJ_TYPE_REF:
			{
				mara_ref_t* ref = (mara_ref_t*)obj->body;
				mara_value_t result = mara_new_ref(ctx, target_zone, ref->tag, ref->value);
				mara_ptr_map_put(ctx, local_zone, copied_objs, obj, mara_value_to_obj(result));
				return result;
			}
		case MARA_OBJ_TYPE_NATIVE_CLOSURE:
			{
				// TODO: Light native representation can skip copying
				mara_native_closure_t* closure = (mara_native_closure_t*)obj->body;
				mara_value_t result = mara_value_from_fn(
					mara_new_fn(ctx, target_zone, closure->fn, closure->options)
				);
				mara_ptr_map_put(ctx, local_zone, copied_objs, obj, mara_value_to_obj(result));
				return result;
			}
		case MARA_OBJ_TYPE_LIST:
			{
				mara_list_t* old_list;
				mara_assert_no_error(mara_value_to_list(ctx, value, &old_list));

				mara_list_t* new_list = mara_new_list(ctx, target_zone, old_list->len);
				mara_obj_t* new_list_header = mara_header_of(new_list);
				mara_ptr_map_put(ctx, local_zone, copied_objs, obj, new_list_header);

				mara_index_t len = new_list->len = old_list->len;
				mara_value_t* old_elems = old_list->elems;
				mara_value_t* new_elems = new_list->elems;
				for (mara_index_t i = 0; i < len; ++i) {
					mara_value_t elem_copy = mara_deep_copy(
						ctx, target_zone, copied_objs, old_elems[i]
					);

					new_elems[i] = elem_copy;
					mara_obj_add_arena_mask(new_list_header, elem_copy);
				}

				return mara_value_from_list(new_list);
			}
		case MARA_OBJ_TYPE_MAP:
			{
				mara_map_t* old_map;
				mara_assert_no_error(mara_value_to_map(ctx, value, &old_map));

				mara_map_t* new_map = mara_new_map(ctx, target_zone);
				mara_ptr_map_put(ctx, local_zone, copied_objs, obj, mara_header_of(new_map));

				for (
					mara_map_node_t* itr = old_map->root;
					itr != NULL;
					itr = itr->next
				) {
					// The copy must be made here for it to be deep
					// If we rely on mara_map_set, it will make a shallow copy
					// starting from the value instead.
					mara_value_t key_copy = mara_deep_copy(ctx, target_zone, copied_objs, itr->key);
					mara_value_t value_copy = mara_deep_copy(ctx, target_zone, copied_objs, itr->value);
					mara_map_set(ctx, new_map, key_copy, value_copy);
				}

				return mara_value_from_map(new_map);
			}
		case MARA_OBJ_TYPE_VM_CLOSURE:
			{
				mara_vm_closure_t* old_closure = (mara_vm_closure_t*)obj->body;
				mara_index_t num_captures = old_closure->fn->num_captures;
				mara_obj_t* new_closure_header = mara_alloc_obj(
					ctx, target_zone,
					sizeof(mara_vm_closure_t) + sizeof(mara_value_t) * num_captures
				);
				new_closure_header->type = MARA_OBJ_TYPE_VM_CLOSURE;
				mara_ptr_map_put(ctx, local_zone, copied_objs, obj, new_closure_header);

				mara_vm_closure_t* new_closure = (mara_vm_closure_t*)new_closure_header->body;
				new_closure->fn = old_closure->fn;
				for (mara_index_t i = 0; i < num_captures; ++i) {
					mara_value_t capture_copy = mara_deep_copy(
						ctx, target_zone, copied_objs,
						old_closure->captures[i]
					);
					new_closure->captures[i] = capture_copy;
					mara_obj_add_arena_mask(new_closure_header, capture_copy);
				}

				return mara_obj_to_value(new_closure_header);
			}
		default:
			return mara_nil();
	}
}

MARA_PRIVATE mara_value_t
mara_start_deep_copy(mara_exec_ctx_t* ctx, mara_zone_t* zone, mara_value_t value) {
	// value is not included because we are not modifying it
	mara_zone_t* copy_zone = mara_zone_enter(ctx, (mara_zone_options_t){
		.return_zone = zone,
	});

	if (MARA_EXPECT(copy_zone != NULL)) {
		mara_ptr_map_t copied_objs = { .root = NULL };
		mara_value_t result = mara_deep_copy(ctx, zone, &copied_objs, value);
		mara_zone_exit(ctx, copy_zone);
		return result;
	} else {
		mara_assert(copy_zone != NULL, "Zone limit reached");
		return mara_nil();
	}
}

mara_value_t
mara_copy(mara_exec_ctx_t* ctx, mara_zone_t* zone, mara_value_t value) {
	if (MARA_EXPECT(!mara_value_is_obj(value))) {
		return value;
	}

	mara_obj_t* obj = mara_value_to_obj(value);
	if (obj->zone->level <= zone->level) {
		return value;
	}

	switch (obj->type) {
		case MARA_OBJ_TYPE_STRING:
			{
				mara_str_t* str = (mara_str_t*)obj->body;
				return mara_new_str(ctx, zone, *str);
			}
		case MARA_OBJ_TYPE_REF:
			{
				mara_ref_t* ref = (mara_ref_t*)obj->body;
				return mara_new_ref(ctx, zone, ref->tag, ref->value);
			}
		case MARA_OBJ_TYPE_NATIVE_CLOSURE:
			{
				mara_native_closure_t* closure = (mara_native_closure_t*)obj->body;
				return mara_value_from_fn(
					mara_new_fn(ctx, zone, closure->fn, closure->options)
				);
			}
		case MARA_OBJ_TYPE_LIST:
		case MARA_OBJ_TYPE_MAP:
		case MARA_OBJ_TYPE_VM_CLOSURE:
			return mara_start_deep_copy(ctx, zone, value);
		default:
			mara_assert(false, "Invalid object type");
			return mara_tombstone();
	}
}
