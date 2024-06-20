#include "internal.h"
#include "xxhash.h"

#define BHAMT_IS_TOMBSTONE(value) false
#define BHAMT_KEYEQ(lhs, rhs) mara_debug_info_key_equal(lhs, rhs)

MARA_PRIVATE bool
mara_debug_info_key_equal(mara_debug_info_key_t lhs, mara_debug_info_key_t rhs) {
	return lhs.container == rhs.container
		&& lhs.index == rhs.index;
}

void
mara_set_debug_info(mara_exec_ctx_t* ctx, mara_source_info_t debug_info) {
	ctx->current_zone->debug_info = debug_info;
}

void
mara_put_debug_info(
	mara_exec_ctx_t* ctx,
	mara_obj_t* container,
	mara_index_t index,
	mara_source_info_t debug_info
) {
	debug_info.filename = mara_strpool_intern(
		ctx->env,
		&ctx->debug_info_arena,
		&ctx->debug_info_strpool,
		debug_info.filename
	);

	mara_debug_info_key_t key;
	// It is not clear whether all compilers zero padding bytes when using
	// initializer.
	// This struct is going to be hashed so just memset it.
	memset(&key, 0, sizeof(mara_debug_info_key_t));
	key.container = container;
	key.index = index;

	mara_debug_info_node_t** itr;
	mara_debug_info_node_t* free_node;
	mara_debug_info_node_t* node;
	(void)free_node;
	BHAMT_HASH_TYPE hash = mara_XXH3_64bits(&key, sizeof(key));
	BHAMT_SEARCH(ctx->debug_info_map.root, itr, node, free_node, hash, key);

	if (node == NULL) {
		node = *itr = MARA_ARENA_ALLOC_TYPE(ctx->env, &ctx->debug_info_arena, mara_debug_info_node_t);
		memset(node->children, 0, sizeof(node->children));
		node->key = key;
		node->debug_info = debug_info;
	}
}

mara_source_info_t*
mara_get_debug_info(
	mara_exec_ctx_t* ctx,
	mara_value_t container,
	mara_value_t index
);
