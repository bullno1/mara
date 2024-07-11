#include "internal.h"
#include "xxhash.h"

#define BHAMT_KEYEQ(lhs, rhs) mara_value_equal(lhs, rhs)
#define BHAMT_IS_TOMBSTONE(value) mara_value_is_tombstone((value)->key)

MARA_PRIVATE BHAMT_HASH_TYPE
mara_hash_value(mara_value_t value) {
	if (mara_value_is_obj(value)) {
		mara_obj_t* obj = mara_value_to_obj(value);
		if (obj->type == MARA_OBJ_TYPE_STRING) {
			mara_str_t* str = (mara_str_t*)obj->body;
			return mara_XXH3_64bits(str->data, str->len);
		} else if (obj->type == MARA_OBJ_TYPE_REF) {
			mara_ref_t* ref = (mara_ref_t*)obj->body;
			return mara_XXH3_64bits(ref, sizeof(*ref));
		} else {
			return mara_XXH3_64bits(&value, sizeof(value));
		}
	} else {
		return mara_XXH3_64bits(&value, sizeof(value));
	}
}

MARA_PRIVATE bool
mara_value_equal(mara_value_t lhs, mara_value_t rhs) {
	if (mara_value_is_obj(lhs)) {
		mara_obj_t* lobj = mara_value_to_obj(lhs);
		if (mara_value_is_obj(rhs)) {
			mara_obj_t* robj = mara_value_to_obj(rhs);
			if (lobj == robj) {
				return true;
			} else if (lobj->type != robj->type) {
				return false;
			} else if (lobj->type == MARA_OBJ_TYPE_STRING) {
				mara_str_t* lstr = (mara_str_t*)lobj->body;
				mara_str_t* rstr = (mara_str_t*)robj->body;
				return mara_str_equal(*lstr, *rstr);
			} else if (lobj->type == MARA_OBJ_TYPE_REF) {
				mara_ref_t* lref = (mara_ref_t*)lobj->body;
				mara_ref_t* rref = (mara_ref_t*)robj->body;
				return (lref->tag == rref->tag)
					&& (lref->value == rref->value);
			} else {
				return false;
			}
		} else {
			return false;
		}
	} else {
		return lhs.internal == rhs.internal;
	}
}

mara_map_t*
mara_new_map(mara_exec_ctx_t* ctx, mara_zone_t* zone) {
	mara_obj_t* obj = mara_alloc_obj(ctx, zone, sizeof(mara_map_t));
	obj->type = MARA_OBJ_TYPE_MAP;

	mara_map_t* map = (mara_map_t*)obj->body;
	*map = (mara_map_t){
		.len = 0,
		.root = NULL,
	};

	return map;
}

mara_index_t
mara_map_len(mara_exec_ctx_t* ctx, mara_map_t* map) {
	(void)ctx;
	return map->len;
}

mara_value_t
mara_map_set(mara_exec_ctx_t* ctx, mara_map_t* map, mara_value_t key, mara_value_t value) {
	if (mara_value_is_nil(value)) {
		return mara_map_delete(ctx, map, key);
	}

	mara_map_node_t** itr;
	mara_map_node_t* head = map->root;
	mara_map_node_t* free_node;
	mara_map_node_t* node;
	BHAMT_HASH_TYPE hash = mara_hash_value(key);
	BHAMT_SEARCH(map->root, itr, node, free_node, hash, key);

	mara_obj_t* header = mara_header_of(map);
	mara_zone_t* map_zone = header->zone;
	mara_value_t old_value = mara_nil();
	if (node == NULL) {
		if (free_node != NULL) {
			node = free_node;
		} else {
			node = *itr = MARA_ZONE_ALLOC_TYPE(ctx, map_zone, mara_map_node_t);
			memset(node->children, 0, sizeof(node->children));
			if (head != NULL) {
				node->next = head->next;
				head->next = node;
			} else {
				node->next = NULL;
			}
		}

		map->len += 1;
		node->key = mara_copy(ctx, map_zone, key);
	} else {
		old_value = node->value;
	}

	node->value = mara_copy(ctx, map_zone, value);
	return old_value;
}

mara_value_t
mara_map_get(mara_exec_ctx_t* ctx, mara_map_t* map, mara_value_t key) {
	(void)ctx;
	mara_map_node_t* node;
	BHAMT_HASH_TYPE hash = mara_hash_value(key);
	BHAMT_GET(map->root, node, hash, key);

	if (node != NULL) {
		return node->value;
	} else {
		return mara_nil();
	}
}

mara_value_t
mara_map_delete(mara_exec_ctx_t* ctx, mara_map_t* map, mara_value_t key) {
	(void)ctx;
	mara_map_node_t* node;
	BHAMT_HASH_TYPE hash = mara_hash_value(key);
	BHAMT_GET(map->root, node, hash, key);

	if (node != NULL) {
		mara_value_t old_value = node->value;
		map->len -= 1;
		node->key = mara_tombstone();
		node->value = mara_nil();
		return old_value;
	} else {
		return mara_nil();
	}
}

mara_error_t*
mara_map_foreach(mara_exec_ctx_t* ctx, mara_map_t* map, mara_fn_t* fn) {
	mara_value_t map_value = mara_value_from_map(map);
	for (
		mara_map_node_t* itr = map->root;
		itr != NULL;
		itr = itr->next
	) {
		if (mara_value_is_tombstone(itr->key)) { continue; }

		mara_value_t args[] = {
			itr->value,
			itr->key,
			map_value
		};
		mara_value_t should_continue = mara_nil();
		mara_check_error(
			mara_call(
				ctx, ctx->current_zone,
				fn, mara_count_of(args), args, &should_continue
			)
		);

		if (mara_value_is_false(should_continue)) {
			break;
		}
	}

	return NULL;
}
