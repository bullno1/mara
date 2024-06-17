#ifndef MARA_INTERNAL_H
#define MARA_INTERNAL_H

#include <mara.h>
#include <mara/utils.h>
#include <assert.h>

#define MARA_PRIVATE static inline
#define MARA_HAMT_HASH_TYPE uint64_t
#define MARA_HAMT_NUM_BITS 3
#define MARA_HAMT_NUM_CHILDREN (1 << MARA_HAMT_NUM_BITS)
#define MARA_HAMT_MASK (((MARA_HAMT_HASH_TYPE)1 << MARA_HAMT_NUM_BITS) - 1)
#define mara_assert(cond, msg) assert((cond) && (msg))

#if defined(__GNUC__) || defined(__clang__)
#	define MARA_EXPECT(X) __builtin_expect(X, true)
#else
#	define MARA_EXPECT(X) (X)
#endif

typedef struct mara_finalizer_s {
	mara_callback_t callback;
	struct mara_finalizer_s* next;
} mara_finalizer_t;

typedef struct mara_arena_chunk_s {
	char* bump_ptr;
	struct mara_arena_chunk_s* next;
	char* end;
	char begin[];
} mara_arena_chunk_t;

typedef struct mara_arena_snapshot_s {
	mara_arena_chunk_t* chunk;
	char* bump_ptr;
} mara_arena_snapshot_t;

typedef struct mara_arena_s {
	struct mara_arena_s* next;
	mara_arena_chunk_t* current_chunk;
	bool in_use;
} mara_arena_t;

typedef enum mara_obj_type_e {
	MARA_OBJ_TYPE_STRING,
	MARA_OBJ_TYPE_SYMBOL,
	MARA_OBJ_TYPE_REF,
	MARA_OBJ_TYPE_MARA_FN,
	MARA_OBJ_TYPE_NATIVE_FN,
	MARA_OBJ_TYPE_LIST,
	MARA_OBJ_TYPE_MAP,
} mara_obj_type_t;

typedef struct mara_obj_s {
	mara_obj_type_t type;
	mara_zone_t* zone;
	_Alignas(max_align_t) char body[];
} mara_obj_t;

typedef struct mara_obj_ref_s {
	void* tag;
	void* value;
} mara_obj_ref_t;

typedef struct mara_obj_list_s {
	bool in_zone;
	mara_index_t len;
	mara_index_t capacity;
	mara_value_t* elems;
} mara_obj_list_t;

typedef struct mara_hamt_node_s {
	struct mara_hamt_node_s* children[MARA_HAMT_NUM_CHILDREN];
	mara_value_t key;
	mara_value_t value;
} mara_hamt_node_t;

typedef struct mara_obj_map_s {
	bool entries_in_sync;
	mara_value_t entries;
	mara_hamt_node_t* root;
} mara_obj_map_t;

typedef struct mara_list_s {
	mara_index_t capacity;
	mara_index_t len;
	mara_value_t* elems;
	bool own_memory;
} mara_list_t;

typedef struct mara_zone_options_s {
	mara_index_t num_marked_zones;
	const mara_zone_t** marked_zones;
	mara_index_t argc;
	const mara_value_t* argv;
} mara_zone_options_t;

typedef struct mara_zone_bookmark_s {
	mara_zone_t* previous_zone;
	struct mara_zone_bookmark_s* previous_bookmark;
	mara_arena_snapshot_t control_snapshot;
} mara_zone_bookmark_t;

typedef enum mara_zone_branch_e {
	MARA_ZONE_BRANCH_MAIN,
	MARA_ZONE_BRANCH_ERROR,
} mara_zone_branch_t;

struct mara_zone_s {
	mara_index_t level;
	mara_index_t ref_count;
	mara_zone_branch_t branch;
	mara_finalizer_t* finalizers;
	mara_arena_t* arena;
	mara_arena_t* ctx_arenas;
	mara_source_info_t debug_info;
	mara_arena_snapshot_t local_snapshot;
	mara_arena_snapshot_t control_snapshot;
};

struct mara_env_s {
	mara_env_options_t options;
	mara_arena_chunk_t* free_chunks;
};

struct mara_exec_ctx_s {
	mara_env_t* env;
	mara_zone_t* current_zone;
	mara_zone_bookmark_t* current_zone_bookmark;
	mara_arena_t* arenas;
	mara_arena_t control_arena;
	mara_error_t last_error;
	mara_zone_t error_zone;
};

// Malloc

void*
mara_malloc(mara_allocator_t* allocator, size_t size);

void
mara_free(mara_allocator_t* allocator, void* ptr);

void*
mara_realloc(mara_allocator_t* allocator, void* ptr, size_t new_size);

void*
mara_arena_alloc(mara_exec_ctx_t* ctx, mara_arena_t* arena, size_t size);

mara_arena_snapshot_t
mara_arena_snapshot(mara_exec_ctx_t* ctx, mara_arena_t* arena);

void
mara_arena_restore(mara_exec_ctx_t* ctx, mara_arena_t* arena, mara_arena_snapshot_t snapshot);

// Zone

mara_zone_t*
mara_zone_new(mara_exec_ctx_t* ctx, mara_zone_options_t options);

void
mara_zone_enter(mara_exec_ctx_t* ctx, mara_zone_t* zone);

void
mara_zone_exit(mara_exec_ctx_t* ctx);

void
mara_zone_cleanup(mara_exec_ctx_t* ctx, mara_zone_t* zone);

void
mara_add_finalizer(mara_exec_ctx_t* ctx, mara_zone_t* zone, mara_callback_t callback);

// Value

mara_obj_t*
mara_alloc_obj(mara_exec_ctx_t* ctx, mara_zone_t* zone, size_t size);

mara_obj_t*
mara_value_to_obj(mara_value_t value);

mara_value_t
mara_obj_to_value(mara_obj_t* obj);

mara_str_t
mara_vsnprintf(mara_exec_ctx_t* ctx, mara_zone_t* zone, const char* fmt, va_list args);

mara_error_t*
mara_type_error(mara_exec_ctx_t* ctx, mara_value_type_t expected, mara_value_t value);

// List

mara_error_t*
mara_unbox_list(mara_exec_ctx_t* ctx, mara_value_t value, mara_obj_list_t** result);

mara_error_t*
mara_raw_list_push(mara_exec_ctx_t* ctx, mara_obj_list_t* list, mara_value_t value);

mara_error_t*
mara_raw_list_set(mara_exec_ctx_t* ctx, mara_obj_list_t* list, mara_index_t index, mara_value_t value);

// Debug

#endif
