#ifndef MARA_INTERNAL_H
#define MARA_INTERNAL_H

#include <mara.h>
#include <mara/utils.h>
#include <assert.h>
#include <limits.h>
#define BHAMT_HASH_TYPE uint64_t
#include "hamt.h"

#define MARA_ARENA_MASK_TYPE uint8_t
#define MARA_NUM_ARENAS (sizeof(MARA_ARENA_MASK_TYPE) * CHAR_BIT)
#define MARA_DEBUG_INFO_SELF ((mara_index_t)-1)

#define MARA_PRIVATE static inline
#define mara_assert(cond, msg) assert((cond) && (msg))

#define mara_assert_no_error(op) \
	do { \
		mara_error_t* error = op; \
		(void)error; \
		mara_assert(error == NULL, "`" #op "` returned error"); \
	} while (0)

#define MARA_ZONE_ALLOC_TYPE(ctx, zone, type) \
	mara_zone_alloc_ex((ctx), (zone), sizeof(type), _Alignof(type))

#define MARA_ARENA_ALLOC_TYPE(env, arena, type) \
	mara_arena_alloc_ex((env), (arena), sizeof(type), _Alignof(type))

#if defined(__GNUC__) || defined(__clang__)
#	define MARA_EXPECT(X) __builtin_expect(!!(X), 1)
#else
#	define MARA_EXPECT(X) (X)
#endif

#if defined(__clang__)
#define MARA_WARNING_PUSH() _Pragma("clang diagnostic push")
#define MARA_WARNING_POP() _Pragma("clang diagnostic pop")
#elif defined(__GNUC__)
#define MARA_WARNING_PUSH() _Pragma("GCC diagnostic push")
#define MARA_WARNING_POP() _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
#define MARA_WARNING_PUSH() _Pragma("warning(push)")
#define MARA_WARNING_POP() _Pragma("warning(pop)")
#else
#define MARA_WARNING_PUSH()
#define MARA_WARNING_POP()
#endif

// Common types

typedef MARA_ARENA_MASK_TYPE mara_arena_mask_t;

typedef struct {
	void (*fn)(mara_env_t* env, void* userdata);
	void* userdata;
} mara_callback_t;

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

typedef struct {
	mara_arena_chunk_t* chunk;
	char* bump_ptr;
} mara_arena_snapshot_t;

typedef struct {
	mara_arena_chunk_t* current_chunk;
} mara_arena_t;

typedef enum mara_obj_type_e {
	MARA_OBJ_TYPE_STRING,
	MARA_OBJ_TYPE_REF,
	MARA_OBJ_TYPE_LIST,
	MARA_OBJ_TYPE_MAP,
	MARA_OBJ_TYPE_MARA_FN,
	MARA_OBJ_TYPE_NATIVE_FN,
} mara_obj_type_t;

typedef struct {
	mara_obj_type_t type;
	mara_arena_mask_t arena_mask;
	mara_zone_t* zone;
	_Alignas(max_align_t) char body[];
} mara_obj_t;

typedef struct {
	void* tag;
	void* value;
} mara_obj_ref_t;

typedef struct {
	bool in_zone;
	mara_index_t len;
	mara_index_t capacity;
	mara_value_t* elems;
} mara_obj_list_t;

typedef struct mara_obj_map_node_s {
	mara_value_t key;
	struct mara_obj_map_node_s* children[BHAMT_NUM_CHILDREN];

	mara_value_t value;
	struct mara_obj_map_node_s* next;
} mara_obj_map_node_t;

typedef struct {
	mara_index_t len;
	mara_obj_map_node_t* root;
} mara_obj_map_t;

typedef struct {
	mara_value_t container;
	mara_index_t index;
} mara_debug_info_key_t;

typedef struct mara_debug_info_node_s {
	mara_debug_info_key_t key;
	struct mara_debug_info_node_s* children[BHAMT_NUM_CHILDREN];

	mara_source_info_t debug_info;
} mara_debug_info_node_t;

typedef struct {
	mara_debug_info_node_t* root;
} mara_debug_info_map_t;

typedef struct mara_strpool_node_s {
	mara_str_t key;
	struct mara_strpool_node_s* children[BHAMT_NUM_CHILDREN];
} mara_strpool_node_t;

typedef struct {
	mara_strpool_node_t* root;
} mara_strpool_t;

typedef struct {
	mara_str_t key;
	mara_index_t children[BHAMT_NUM_CHILDREN];
} mara_symtab_node_t;

typedef struct {
	mara_index_t capacity;
	mara_index_t len;
	mara_symtab_node_t* nodes;
} mara_symtab_t;

typedef struct {
	mara_index_t capacity;
	mara_index_t len;
	mara_value_t* elems;
	bool own_memory;
} mara_list_t;

typedef struct {
	mara_arena_snapshot_t arena_snapshot;
	mara_finalizer_t* finalizers;
} mara_zone_snapshot_t;

typedef struct {
	mara_index_t num_marked_zones;
	mara_zone_t** marked_zones;
	mara_index_t argc;
	const mara_value_t* argv;
} mara_zone_options_t;

typedef struct mara_zone_bookmark_s {
	mara_zone_t* previous_zone;
	struct mara_zone_bookmark_s* previous_bookmark;
	mara_arena_snapshot_t control_snapshot;
} mara_zone_bookmark_t;

typedef struct mara_module_loader_entry_s {
	struct mara_module_loader_entry_s* next;

	mara_value_t loader;
} mara_module_loader_entry_t;

// VM types

typedef enum mara_opcode_e {
	MARA_OP_NOP,

	MARA_OP_NIL,
	MARA_OP_TRUE,
	MARA_OP_FALSE,
	MARA_OP_SMALL_INT,

	MARA_OP_CONSTANT,
	MARA_OP_POP,

	MARA_OP_SET_LOCAL,
	MARA_OP_GET_LOCAL,
	MARA_OP_SET_ARG,
	MARA_OP_GET_ARG,
	MARA_OP_SET_CAPTURE,
	MARA_OP_GET_CAPTURE,

	MARA_OP_CALL,
	MARA_OP_RETURN,
	MARA_OP_JUMP,
	MARA_OP_JUMP_IF_FALSE,

	MARA_OP_MAKE_CLOSURE,  // Should arena mask the closure
} mara_opcode_t;

// 8 bits for opcode
// 24 bits for operands
typedef uint32_t mara_instruction_t;
typedef uint32_t mara_operand_t;

typedef struct mara_function_s {
	mara_index_t num_args;
	mara_index_t num_locals;
	mara_index_t num_captures;
	mara_index_t stack_size;
	mara_index_t num_instructions;
	mara_instruction_t* instructions;

	mara_str_t filename;
	mara_source_info_t* source_info;

	mara_index_t num_constants;
	mara_value_t* constants;

	mara_index_t num_functions;
	struct mara_function_s** functions;
} mara_function_t;

typedef struct mara_obj_closure_s {
	mara_function_t* fn;
	mara_value_t captures[];
} mara_obj_closure_t;

typedef struct mara_stack_frame_s mara_stack_frame_t;

typedef struct mara_vm_state_s {
	mara_stack_frame_t* fp;
	mara_instruction_t* ip;
	mara_value_t* sp;
	mara_value_t* args;
} mara_vm_state_t;

struct mara_stack_frame_s {
	mara_obj_closure_t* closure;

	mara_zone_bookmark_t* zone_bookmark;
	mara_vm_state_t saved_state;

	mara_value_t stack[];
};

// Public types

struct mara_zone_s {
	mara_index_t level;
	mara_index_t ref_count;
	mara_finalizer_t* finalizers;
	mara_arena_t* arena;
	mara_source_info_t debug_info;
	mara_arena_snapshot_t local_snapshot;
	mara_arena_snapshot_t control_snapshot;
};

struct mara_env_s {
	mara_env_options_t options;
	mara_arena_chunk_t* free_chunks;
	mara_value_t module_cache;
	mara_zone_t permanent_zone;
	mara_arena_t permanent_arena;
	mara_strpool_t permanent_strpool;
	mara_symtab_t symtab;
};

struct mara_exec_ctx_s {
	mara_env_t* env;
	mara_zone_t* current_zone;
	mara_zone_bookmark_t* current_zone_bookmark;
	mara_arena_t control_arena;
	mara_arena_t arenas[MARA_NUM_ARENAS];

	mara_arena_t error_arena;
	mara_error_t last_error;
	mara_zone_t error_zone;

	mara_value_t current_module;
	mara_module_options_t current_module_options;
	mara_module_loader_entry_t* first_loader;
	mara_module_loader_entry_t* last_loader;

	mara_arena_t debug_info_arena;
	mara_strpool_t debug_info_strpool;
	mara_debug_info_map_t debug_info_map;

	mara_vm_state_t vm_state;
};

// Malloc

void*
mara_malloc(mara_allocator_t allocator, size_t size);

void
mara_free(mara_allocator_t allocator, void* ptr);

void*
mara_realloc(mara_allocator_t allocator, void* ptr, size_t new_size);

void*
mara_arena_alloc(mara_env_t* env, mara_arena_t* arena, size_t size);

void*
mara_arena_alloc_ex(mara_env_t* env, mara_arena_t* arena, size_t size, size_t align);

mara_arena_snapshot_t
mara_arena_snapshot(mara_env_t* env, mara_arena_t* arena);

void
mara_arena_restore(mara_env_t* env, mara_arena_t* arena, mara_arena_snapshot_t snapshot);

void
mara_arena_reset(mara_env_t* env, mara_arena_t* arena);

// Zone

void
mara_zone_enter_new(mara_exec_ctx_t* ctx, mara_zone_options_t options);

void
mara_zone_enter(mara_exec_ctx_t* ctx, mara_zone_t* zone);

void
mara_zone_exit(mara_exec_ctx_t* ctx);

void
mara_zone_cleanup(mara_env_t* env, mara_zone_t* zone);

void
mara_add_finalizer(mara_exec_ctx_t* ctx, mara_zone_t* zone, mara_callback_t callback);

mara_arena_mask_t
mara_arena_mask_of_zone(mara_exec_ctx_t* ctx, mara_zone_t* zone);

mara_zone_snapshot_t
mara_zone_snapshot(mara_exec_ctx_t* ctx);

void
mara_zone_restore(mara_exec_ctx_t* ctx, mara_zone_snapshot_t snapshot);

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

mara_value_t
mara_tombstone(void);

bool
mara_value_is_tombstone(mara_value_t value);

void
mara_obj_add_arena_mask(mara_obj_t* parent, mara_value_t child);

MARA_PRIVATE const char*
mara_value_type_name(mara_value_type_t type) {
	switch (type) {
		case MARA_VAL_NIL:
			return "nil";
		case MARA_VAL_INT:
			return "int";
		case MARA_VAL_REAL:
			return "real";
		case MARA_VAL_BOOL:
			return "bool";
		case MARA_VAL_STRING:
			return "string";
		case MARA_VAL_SYMBOL:
			return "symbol";
		case MARA_VAL_REF:
			return "ref";
		case MARA_VAL_FUNCTION:
			return "function";
		case MARA_VAL_LIST:
			return "list";
		case MARA_VAL_MAP:
			return "map";
		default:
			mara_assert(false, "Invalid type");
			return "";
	}
}

// List

mara_error_t*
mara_unbox_list(mara_exec_ctx_t* ctx, mara_value_t value, mara_obj_list_t** result);

// Map

mara_error_t*
mara_unbox_map(mara_exec_ctx_t* ctx, mara_value_t value, mara_obj_map_t** result);

// Symbol table

void
mara_symtab_init(mara_env_t* env, mara_symtab_t* symtab);

mara_index_t
mara_symtab_intern(mara_env_t* env, mara_symtab_t* symtab, mara_str_t string);

mara_str_t
mara_symtab_lookup(mara_symtab_t* symtab, mara_index_t id);

void
mara_symtab_cleanup(mara_env_t* env, mara_symtab_t* symtab);

// Debug

mara_debug_info_key_t
mara_make_debug_info_key(mara_value_t container, mara_index_t index);

void
mara_put_debug_info(
	mara_exec_ctx_t* ctx,
	mara_debug_info_key_t key,
	mara_source_info_t debug_info
);

const mara_source_info_t*
mara_get_debug_info(mara_exec_ctx_t* ctx, mara_debug_info_key_t key);

mara_str_t
mara_strpool_intern(
	mara_env_t* env,
	mara_arena_t* arena,
	mara_strpool_t* strpool,
	mara_str_t str
);

// VM

#endif
