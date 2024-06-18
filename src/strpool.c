#include "internal.h"
#include "vendor/xxhash.h"

mara_index_t
mara_strpool_intern(mara_allocator_t* allocator, mara_strpool_t* strpool, mara_str_t string) {
	mara_index_t index_itr = strpool->nodes != NULL ? 0 : -1;
	uint64_t hash_itr = XXH3_64bits(string.data, string.len);
	mara_strpool_node_t* node = NULL;
	for (; index_itr >= 0; hash_itr >>= BHAMT_NUM_BITS) {
		node = &strpool->nodes[index_itr];
		if (mara_str_equal(node->key, string)) {
			break;
		}

		// Offset all indicies by 1 so we can zero the new node
		index_itr = node->children[hash_itr & BHAMT_MASK] - 1;
	}

	if (index_itr >= 0) {
		return index_itr;
	} else {
		mara_index_t current_len = strpool->len;
		mara_index_t current_capacity = strpool->capacity;
		if (current_len >= strpool->capacity) {
			mara_index_t new_capacity = current_capacity > 0 ? current_capacity * 2 : 4;
			strpool->nodes = mara_realloc(allocator, strpool->nodes, sizeof(mara_strpool_node_t) * new_capacity);
			mara_assert(strpool->nodes != NULL, "Out of memory");
			strpool->capacity = new_capacity;
		}

		mara_strpool_node_t* new_node = &strpool->nodes[strpool->len++];

		memset(new_node->children, 0, sizeof(new_node->children));
		char* chars = mara_malloc(allocator, string.len);
		mara_assert(chars != NULL, "Out of memory");
		memcpy(chars, string.data, string.len);
		new_node->key = (mara_str_t){
			.len = string.len,
			.data = chars,
		};

		if (node != NULL) {
			node->children[hash_itr & BHAMT_MASK] = current_len + 1;
		}

		return current_len;
	}
}

mara_str_t
mara_strpool_lookup(mara_strpool_t* strpool, mara_index_t id) {
	return strpool->nodes[id].key;
}

void
mara_strpool_cleanup(mara_allocator_t* allocator, mara_strpool_t* strpool) {
	mara_index_t len = strpool->len;
	for (mara_index_t i = 0; i < len; ++i) {
		mara_free(allocator, (void*)strpool->nodes[i].key.data);
	}
	mara_free(allocator, strpool->nodes);
	memset(strpool, 0, sizeof(*strpool));
}
