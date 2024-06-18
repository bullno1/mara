#include "internal.h"
#include "vendor/xxhash.h"
#include "vendor/nanbox.h"

#define BHAMT_KEYEQ(lhs, rhs) mara_value_equal(lhs, rhs)
#define BHAMT_IS_TOMBSTONE(value) nanbox_is_deleted((nanbox_t){ .as_int64 = (value)->key })

MARA_PRIVATE BHAMT_HASH_TYPE
mara_hash_value(mara_value_t value) {
	mara_obj_t* obj = mara_value_to_obj(value);
	if (obj == NULL) {
		return XXH3_64bits(&value, sizeof(value));
	} else {
		if (obj->type == MARA_OBJ_TYPE_STRING) {
			mara_str_t* str = (mara_str_t*)obj->body;
			return XXH3_64bits(str->data, str->len);
		} else if (obj->type == MARA_OBJ_TYPE_REF) {
			mara_obj_ref_t* ref = (mara_obj_ref_t*)obj->body;
			return XXH3_64bits(ref, sizeof(*ref));
		} else {
			return XXH3_64bits(&value, sizeof(value));
		}
	}
}

MARA_PRIVATE bool
mara_value_equal(mara_value_t lhs, mara_value_t rhs) {
	mara_obj_t* lobj = mara_value_to_obj(lhs);
	if (lobj == NULL) {
		return lhs == rhs;
	} else {
		mara_obj_t* robj = mara_value_to_obj(rhs);
		if (lobj == robj) {
			return true;
		} else if (robj == NULL) {
			return false;
		} else if (lobj->type != robj->type) {
			return false;
		} else if (lobj->type == MARA_OBJ_TYPE_STRING) {
			mara_str_t* lstr = (mara_str_t*)lobj->body;
			mara_str_t* rstr = (mara_str_t*)robj->body;
			return mara_str_equal(*lstr, *rstr);
		} else if (lobj->type == MARA_OBJ_TYPE_REF) {
			mara_obj_ref_t* lref = (mara_obj_ref_t*)lobj->body;
			mara_obj_ref_t* rref = (mara_obj_ref_t*)robj->body;
			return (lref->tag == rref->tag)
				&& (lref->value == rref->value);
		} else {
			return false;
		}
	}
}

MARA_PRIVATE mara_error_t*
mara_unbox_map(mara_exec_ctx_t* ctx, mara_value_t value, mara_obj_map_t** result) {
	if (MARA_EXPECT(mara_value_is_map(value))) {
		*result = (mara_obj_map_t*)(mara_value_to_obj(value)->body);
		return NULL;
	} else {
		return mara_type_error(ctx, MARA_VAL_MAP, value);
	}
}

mara_value_t
mara_new_map(mara_exec_ctx_t* ctx, mara_zone_t* zone) {
	mara_obj_t* obj = mara_alloc_obj(ctx, zone, sizeof(mara_obj_map_t));
	obj->type = MARA_OBJ_TYPE_MAP;

	mara_obj_map_t* map = (mara_obj_map_t*)obj->body;
	*map = (mara_obj_map_t){
		.len = 0,
		.root = NULL,
	};

	return mara_obj_to_value(obj);
}

mara_error_t*
mara_map_len(mara_exec_ctx_t* ctx, mara_value_t map, mara_index_t* result) {
	mara_obj_map_t* obj;
	mara_check_error(mara_unbox_map(ctx, map, &obj));
	*result = obj->len;
	return NULL;
}

mara_error_t*
mara_map_set(mara_exec_ctx_t* ctx, mara_value_t map, mara_value_t key, mara_value_t value) {
	if (mara_value_is_null(value)) {
		mara_value_t result;
		return mara_map_delete(ctx, map, key, &result);
	}

	mara_obj_map_t* obj;
	mara_check_error(mara_unbox_map(ctx, map, &obj));

	mara_hamt_node_t** itr;
	mara_hamt_node_t* head = obj->root;
	mara_hamt_node_t* free_node;
	mara_hamt_node_t* node;
	BHAMT_HASH_TYPE hash = mara_hash_value(key);
	BHAMT_SEARCH(obj->root, itr, node, free_node, hash, key);

	mara_zone_t* map_zone = mara_container_of(obj, mara_obj_t, body)->zone;
	if (node == NULL) {
		if (free_node != NULL) {
			node = free_node;
		} else {
			node = *itr = mara_zone_alloc(
				ctx,
				map_zone,
				sizeof(mara_hamt_node_t)
			);
			memset(node->children, 0, sizeof(node->children));
			if (head != NULL) {
				node->next = head;
				head->next = node;
			} else {
				node->next = NULL;
			}
		}

		obj->len += 1;
		mara_check_error(mara_copy(ctx, map_zone, key, &node->key));
	}

	return mara_copy(ctx, map_zone, value, &node->value);
}

mara_error_t*
mara_map_get(mara_exec_ctx_t* ctx, mara_value_t map, mara_value_t key, mara_value_t* result) {
	mara_obj_map_t* obj;
	mara_check_error(mara_unbox_map(ctx, map, &obj));

	mara_hamt_node_t* node;
	BHAMT_HASH_TYPE hash = mara_hash_value(key);
	BHAMT_GET(obj->root, node, hash, key);

	if (node != NULL) {
		*result = node->value;
	} else {
		*result = mara_null();
	}

	return NULL;
}

mara_error_t*
mara_map_delete(mara_exec_ctx_t* ctx, mara_value_t map, mara_value_t key, mara_value_t* result) {
	mara_obj_map_t* obj;
	mara_check_error(mara_unbox_map(ctx, map, &obj));

	mara_hamt_node_t* node;
	BHAMT_HASH_TYPE hash = mara_hash_value(key);
	BHAMT_GET(obj->root, node, hash, key);

	if (node != NULL) {
		obj->len -= 1;
		node->key = nanbox_deleted().as_int64;
		*result = mara_value_from_bool(true);
	} else {
		*result = mara_value_from_bool(false);
	}

	return NULL;
}
