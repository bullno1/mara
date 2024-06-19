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

#define MARA_PRIVATE static inline
#define mara_assert(cond, msg) assert((cond) && (msg))

#define MARA_ZONE_ALLOC_TYPE(ctx, zone, type) \
	mara_zone_alloc_ex((ctx), (zone), sizeof(type), _Alignof(type))

#define MARA_ARENA_ALLOC_TYPE(env, arena, type) \
	mara_arena_alloc_ex((env), (arena), sizeof(type), _Alignof(type))

#if defined(__GNUC__) || defined(__clang__)
#	define MARA_EXPECT(X) __builtin_expect(!!(X), 1)
#else
#	define MARA_EXPECT(X) (X)
#endif

// Common types

typedef MARA_ARENA_MASK_TYPE mara_arena_mask_t;

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

typedef struct mara_obj_s {
	mara_obj_type_t type;
	mara_arena_mask_t arena_mask;
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

typedef struct mara_obj_map_node_s {
	mara_value_t key;
	struct mara_obj_map_node_s* children[BHAMT_NUM_CHILDREN];

	mara_value_t value;
	struct mara_obj_map_node_s* next;
} mara_obj_map_node_t;

typedef struct mara_obj_map_s {
	mara_index_t len;
	mara_obj_map_node_t* root;
} mara_obj_map_t;

typedef struct mara_strpool_node_s {
	mara_str_t key;
	mara_index_t children[BHAMT_NUM_CHILDREN];
} mara_strpool_node_t;

typedef struct mara_strpool_options_s {
	mara_env_t* env;
	mara_allocator_t table_allocator;
	mara_arena_t* string_arena;
} mara_strpool_options_t;

typedef struct mara_strpool_s {
	mara_strpool_options_t options;
	mara_index_t capacity;
	mara_index_t len;
	mara_strpool_node_t* nodes;
} mara_strpool_t;

typedef struct mara_list_s {
	mara_index_t capacity;
	mara_index_t len;
	mara_value_t* elems;
	bool own_memory;
} mara_list_t;

typedef struct mara_zone_snapshot_s {
	mara_arena_snapshot_t arena_snapshot;
	mara_finalizer_t* finalizers;
} mara_zone_snapshot_t;

typedef struct mara_zone_options_s {
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

typedef enum mara_zone_branch_e {
	MARA_ZONE_BRANCH_MAIN,
	MARA_ZONE_BRANCH_ERROR,
} mara_zone_branch_t;

// VM types

typedef enum mara_opcode_e {
	MARA_OP_NOP,

	MARA_OP_NIL,
	MARA_OP_TRUE,
	MARA_OP_FALSE,
	MARA_OP_SMALL_INT,

	MARA_OP_LOAD,
	MARA_OP_POP,

	MARA_OP_SET_LOCAL,
	MARA_OP_GET_LOCAL,
	MARA_OP_SET_ARG,
	MARA_OP_GET_ARG,
	MARA_OP_SET_CAPTURE,
	MARA_OP_GET_CAPTURE,

	MARA_OP_CALL,
	MARA_OP_RETURN,
	MARA_OP_TAIL_CALL,
	MARA_OP_JUMP,
	MARA_OP_JUMP_IF_FALSE,

	MARA_OP_ZONE_ENTER,
	MARA_OP_ZONE_EXIT,

	MARA_OP_MAKE_CLOSURE,  // Should arena mask the closure
} mara_opcode_t;

// 8 bits for opcode
// 24 bits for operands
typedef uint32_t mara_instruction_t;
typedef uint32_t mara_operand_t;

typedef struct mara_function_s {
	mara_index_t num_args;
	mara_index_t num_captures;
	mara_index_t stack_size;
	mara_index_t num_instructions;
	mara_instruction_t* instructions;

	mara_str_t filename;
	mara_source_range_t* source_info;

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
	mara_value_t* bp;
} mara_vm_state_t;

struct mara_stack_frame_s {
	mara_obj_closure_t* closure;

	mara_source_info_t* source_info;
	mara_zone_bookmark_t* zone_bookmark;
	mara_vm_state_t saved_state;

	mara_value_t stack[];
};

// Public types

struct mara_zone_s {
	mara_index_t level;
	mara_index_t ref_count;
	mara_zone_branch_t branch;
	mara_finalizer_t* finalizers;
	mara_arena_t* arena;
	mara_source_info_t debug_info;
	mara_arena_snapshot_t local_snapshot;
	mara_arena_snapshot_t control_snapshot;
};

struct mara_env_s {
	mara_env_options_t options;
	mara_arena_chunk_t* free_chunks;
	mara_arena_t permanent_arena;
	mara_strpool_t symtab;
};

struct mara_exec_ctx_s {
	mara_env_t* env;
	mara_zone_t* current_zone;
	mara_zone_bookmark_t* current_zone_bookmark;
	mara_arena_t control_arena;
	mara_error_t last_error;
	mara_zone_t error_zone;
	mara_arena_t arenas[MARA_NUM_ARENAS];

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
mara_zone_cleanup(mara_exec_ctx_t* ctx, mara_zone_t* zone);

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

// String pool

void
mara_strpool_init(mara_strpool_t* strpool, mara_strpool_options_t options);

mara_index_t
mara_strpool_intern(mara_strpool_t* strpool, mara_str_t string);

mara_str_t
mara_strpool_lookup(mara_strpool_t* strpool, mara_index_t id);

void
mara_strpool_cleanup(mara_strpool_t* strpool);

// Debug

// VM

MARA_PRIVATE const char*
mara_opcode_to_str(mara_opcode_t opcode) {
	switch (opcode) {
		case MARA_OP_NOP:
			return "NOP";
		case MARA_OP_NIL:
			return "NIL";
		case MARA_OP_TRUE:
			return "TRUE";
		case MARA_OP_FALSE:
			return "FALSE";
		case MARA_OP_SMALL_INT:
			return "SMALL_INT";
		case MARA_OP_LOAD:
			return "LOAD";
		case MARA_OP_POP:
			return "POP";
		case MARA_OP_SET_LOCAL:
			return "SET_LOCAL";
		case MARA_OP_GET_LOCAL:
			return "GET_LOCAL";
		case MARA_OP_SET_ARG:
			return "SET_ARG";
		case MARA_OP_GET_ARG:
			return "GET_ARG";
		case MARA_OP_SET_CAPTURE:
			return "SET_CAPTURE";
		case MARA_OP_GET_CAPTURE:
			return "GET_CAPTURE";
		case MARA_OP_CALL:
			return "CALL";
		case MARA_OP_RETURN:
			return "RETURN";
		case MARA_OP_TAIL_CALL:
			return "TAIL_CALL";
		case MARA_OP_JUMP:
			return "JUMP";
		case MARA_OP_JUMP_IF_FALSE:
			return "JUMP_IF_FALSE";
		case MARA_OP_ZONE_ENTER:
			return "ZONE_ENTER";
		case MARA_OP_ZONE_EXIT:
			return "ZONE_EXIT";
		case MARA_OP_MAKE_CLOSURE:
			return "MAKE_CLOSURE";
	}

	return NULL;
}

#endif
