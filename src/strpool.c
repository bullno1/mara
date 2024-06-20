#include "internal.h"
#include "xxhash.h"

#define BHAMT_IS_TOMBSTONE(value) false
#define BHAMT_KEYEQ(lhs, rhs) mara_str_equal(lhs, rhs)

mara_str_t
mara_strpool_intern(
	mara_env_t* env,
	mara_arena_t* arena,
	mara_strpool_t* strpool,
	mara_str_t str
) {
	mara_strpool_node_t** itr;
	mara_strpool_node_t* free_node;
	mara_strpool_node_t* node;
	(void)free_node;
	BHAMT_HASH_TYPE hash = mara_XXH3_64bits(str.data, str.len);
	BHAMT_SEARCH(strpool->root, itr, node, free_node, hash, str);

	if (node != NULL) {
		return node->key;
	} else {
		node = *itr = MARA_ARENA_ALLOC_TYPE(env, arena, mara_strpool_node_t);
		memset(node->children, 0, sizeof(node->children));
		char* chars = mara_arena_alloc_ex(env, arena, str.len, _Alignof(char));
		memcpy(chars, str.data, str.len);
		node->key = (mara_str_t){
			.len = str.len,
			.data = chars,
		};
		return node->key;
	}
}
