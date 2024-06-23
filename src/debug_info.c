#include "internal.h"
#include "xxhash.h"

#define BHAMT_IS_TOMBSTONE(value) false
#define BHAMT_KEYEQ(lhs, rhs) mara_debug_info_key_equal(lhs, rhs)

MARA_PRIVATE bool
mara_debug_info_key_equal(mara_debug_info_key_t lhs, mara_debug_info_key_t rhs) {
	return lhs.container.internal == rhs.container.internal
		&& lhs.index == rhs.index;
}

void
mara_set_debug_info(mara_exec_ctx_t* ctx, mara_source_info_t debug_info) {
	ctx->current_zone->debug_info = debug_info;
	mara_stack_frame_t* current_frame = ctx->vm_state.fp;
	if (current_frame != NULL) {
		current_frame->native_debug_info = debug_info;
	}
}

MARA_PRIVATE void
normalize_key(mara_value_t container, mara_index_t index, mara_debug_info_key_t* key) {
	// Padding bytes might not be zeroed when using initializer
	memset(key, 0, sizeof(*key));
	key->container = container;
	key->index = index;
}

void
mara_put_debug_info(
	mara_exec_ctx_t* ctx,
	mara_debug_info_key_t key,
	mara_source_info_t debug_info
) {
	debug_info.filename = mara_strpool_intern(
		ctx->env,
		&ctx->debug_info_arena,
		&ctx->debug_info_strpool,
		debug_info.filename
	);

	mara_debug_info_node_t** itr;
	mara_debug_info_node_t* free_node;
	mara_debug_info_node_t* node;
	(void)free_node;
	mara_debug_info_key_t normalized_key;
	normalize_key(key.container, key.index, &normalized_key);
	BHAMT_HASH_TYPE hash = mara_XXH3_64bits(&normalized_key, sizeof(normalized_key));
	BHAMT_SEARCH(ctx->debug_info_map.root, itr, node, free_node, hash, key);

	if (node == NULL) {
		node = *itr = MARA_ARENA_ALLOC_TYPE(ctx->env, &ctx->debug_info_arena, mara_debug_info_node_t);
		memset(node->children, 0, sizeof(node->children));
		node->key = key;
		node->debug_info = debug_info;
	}
}

const mara_source_info_t*
mara_get_debug_info(mara_exec_ctx_t* ctx, mara_debug_info_key_t key) {
	if (mara_value_is_nil(key.container)) { return NULL; }

	mara_debug_info_node_t* node;
	mara_debug_info_key_t normalized_key;
	normalize_key(key.container, key.index, &normalized_key);
	BHAMT_HASH_TYPE hash = mara_XXH3_64bits(&normalized_key, sizeof(normalized_key));
	BHAMT_GET(ctx->debug_info_map.root, node, hash, key);

	return node != NULL ? &node->debug_info : NULL;
}

mara_debug_info_key_t
mara_make_debug_info_key(mara_value_t container, mara_index_t index) {
	// Padding bytes will be zeroed in the respective get/set function instead
	return (mara_debug_info_key_t){
		.container = container,
		.index = index,
	};
}
