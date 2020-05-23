#ifndef MARA_INTERNAL_H
#define MARA_INTERNAL_H

#include <mara.h>
#include <ugc/ugc.h>
#include <bk/allocator.h>
#include <limits.h>

#if INTPTR_MAX == INT32_MAX
#	define MARA_HASH_BITS 32
#elif INTPTR_MAX == INT64_MAX
#	define MARA_HASH_BITS 64
#else
#	define MARA_HASH_BITS 32
#endif

#define MARA_STACK_MAX UINT8_MAX
#define MARA_RECORD_ATTR_MAX UINT8_MAX

#define MARA_HASH(CTX, DATA, SIZE) \
	BK_PP_CONCAT(XXH, MARA_HASH_BITS)((DATA), (SIZE), (uintptr_t)(CTX))
#define MARA_HASH_TYPE \
	BK_PP_CONCAT(BK_PP_CONCAT(uint, MARA_HASH_BITS), _t)

#define MARA_ASSERT(ctx, condition, message) \
	do { \
		if(!(condition)) { \
			ctx->config.panic_handler(ctx, message, __FILE__, __LINE__); \
		} \
	} while(0);

#define MARA_AS_GC_TYPE(CTX, INDEX, GC_TYPE) \
	BK_CONTAINER_OF( \
		mara_value_as_gc_obj(mara_stack_addr((CTX), (INDEX))), \
		GC_TYPE, \
		gc_header \
	)

#define MARA_GC_OBJ_TYPE(X) \
	X(MARA_GC_STRING) \
	X(MARA_GC_SYMBOL) \
	X(MARA_GC_LIST) \
	X(MARA_GC_RECORD) \
	X(MARA_GC_RECORD_INFO) \
	X(MARA_GC_THREAD) \
	X(MARA_GC_FUNCTION_CLOSURE) \
	X(MARA_GC_FUNCTION_PROTOTYPE) \
	X(MARA_GC_UPVALUE) \
	X(MARA_GC_HANDLE) \

BK_ENUM(mara_gc_obj_type_t, MARA_GC_OBJ_TYPE)

#define MARA_VAL(X) \
	X(MARA_VAL_NULL) \
	X(MARA_VAL_BOOL) \
	X(MARA_VAL_NUMBER) \
	X(MARA_VAL_GC_OBJ)

BK_ENUM(mara_value_type_t, MARA_VAL)

#if MARA_NANBOX
#include "value_nanbox.h"
#else
#include "value_union.h"
#endif

#define MARA_HASH_SWAP(TYPE, ARRAY, INDEX1, INDEX2) \
	do { \
		TYPE tmp = ARRAY[INDEX1]; \
		ARRAY[INDEX1] = ARRAY[INDEX2]; \
		ARRAY[INDEX2] = tmp; \
	} while(0)

typedef MARA_HASH_TYPE mara_hash_t;
typedef struct mara_gc_header_s mara_gc_header_t;
typedef struct mara_string_s mara_string_t;
typedef struct mara_symbol_s mara_symbol_t;
typedef struct mara_list_s mara_list_t;
typedef struct mara_record_s mara_record_t;
typedef struct mara_record_info_s mara_record_info_t;
typedef struct mara_function_closure_s mara_function_closure_t;
typedef struct mara_function_prototype_s mara_function_prototype_t;
typedef struct mara_upvalue_s mara_upvalue_t;
typedef struct mara_strpool_s mara_strpool_t;
typedef struct mara_stack_frame_s mara_stack_frame_t;
typedef uint32_t mara_instruction_t;
typedef void(*mara_gc_visit_fn_t)(mara_context_t* ctx, mara_gc_header_t* header);

struct mara_strpool_s
{
	mara_string_t** strings;
	size_t size;
	size_t capacity;
};

struct mara_context_s
{
	bk_allocator_t allocator;
	mara_context_config_t config;

	ugc_t gc;
	size_t gc_mem;

	mara_strpool_t symtab;
	mara_thread_t* main_thread;
	mara_thread_t* current_thread;
};

struct mara_gc_header_s
{
	ugc_header_t ugc_header;
	mara_gc_obj_type_t obj_type;
};

struct mara_string_s
{
	mara_gc_header_t gc_header;

	mara_hash_t hash;
	size_t length;
	char data[];
};

struct mara_native_location_s
{
	const char* file;
	unsigned int line;
};

struct mara_record_attr_info_s
{
	mara_string_t* name;
	uint8_t slot;
};

struct mara_record_info_s
{
	mara_gc_header_t gc_header;

	mara_hash_t seed;
	const mara_record_decl_t* decl;

	mara_string_t* name;
	uint8_t num_attrs;
	struct mara_record_attr_info_s attr_infos[];
};

struct mara_record_s
{
	mara_gc_header_t gc_header;

	mara_record_info_t* record_info;
	mara_value_t attributes[];
};

struct mara_function_prototype_s
{
	mara_gc_header_t gc_header;

	unsigned is_vararg: 1;
	unsigned num_args: 7;
	unsigned num_upvalues: 8;
	unsigned num_instructions: 16;

	unsigned num_locals: 8;
	unsigned num_literals: 16;

	mara_instruction_t* instructions;
	mara_source_range_t* source_locations;
	mara_string_t* file;

	mara_value_t literals[];
};

struct mara_upvalue_s
{
	mara_value_t* value_ref;
	mara_value_t own_value;
};

struct mara_function_closure_s
{
	mara_gc_header_t gc_header;
	struct mara_function_prototype_s* prototype;

	mara_upvalue_t* upvalues[];
};

struct mara_stack_frame_s
{
	mara_value_t* base_pointer;
	mara_value_t* stack_pointer;

	bool is_native;

	union
	{
		struct {
			mara_instruction_t* instruction_pointer;
			mara_function_closure_t* closure;
		} script;

		struct {
			mara_string_ref_t filename;
			unsigned int line;
		} native;
	} data;
};

struct mara_thread_s
{
	mara_gc_header_t gc_header;

	mara_string_t* name;
	mara_stack_frame_t* frame_pointer;

	mara_stack_frame_t* frame_pointer_min;

	// Memory layout of stack[]:
	//
	// [maybe padding]          <- &stack[0]
	// [mara_stack_frame_t]     <- frame_pointer_min points to the first frame
	// [mara_stack_frame_t]        This frame is always a native frame since
	// [mara_stack_frame_t]        VM starts from native code.
	// [mara_stack_frame_t]
	// [mara_stack_frame_t]     <- frame_pointer points to the current frame
	// ...
	// ...                      <- When the two regions meet in the middle, a
	// ...                         stack overflow occurs.
	// ...
	// [mara_value_t]           <- frame_pointer->stack_pointer points to the
	// [mara_value_t]              next element to be used in the stack
	// [mara_value_t]
	// [mara_value_t]           <- frame_pointer->base_pointer points to the
	// [mara_value_t]              first element in the current stack frame
	// [mara_value_t]
	// [mara_value_t]           <- frame_pointer_min->base_pointer
	// [maybe extra]            <- &stack[stack_size]
	size_t stack_size;
	char stack[];
};


void
mara_gc_mark(mara_context_t* ctx, mara_gc_header_t* header);

void
mara_gc_write_barrier(
	mara_context_t* ctx,
	mara_gc_header_t* parent,
	mara_gc_header_t* child
);

void
mara_gc_register(mara_context_t* ctx, mara_gc_header_t* header);

void
mara_value_set_null(mara_value_t* value);

void
mara_value_set_number(mara_value_t* value, mara_number_t num);

void
mara_value_set_bool(mara_value_t* value, bool boolean);

// Use mara_write_value_ref instead
void
mara_value_set_gc_obj(mara_value_t* value, mara_gc_header_t* obj);

bool
mara_value_type_check(mara_value_t* value, mara_value_type_t type);

mara_number_t
mara_value_as_number(mara_value_t* value);

bool
mara_value_as_bool(mara_value_t* value);

mara_gc_header_t*
mara_value_as_gc_obj(mara_value_t* value);

mara_value_t*
mara_stack_addr(mara_context_t* ctx, mara_index_t index);

void
mara_push_gc_obj(mara_context_t* ctx, mara_gc_header_t* gc_obj);


static inline void*
mara_malloc(mara_context_t* ctx, size_t size)
{
	return bk_malloc(&ctx->allocator, size);
}

static inline void
mara_free(mara_context_t* ctx, void* ptr)
{
	bk_free(&ctx->allocator, ptr);
}

static inline void
mara_write_value_ref(
	mara_context_t* ctx,
	mara_gc_header_t* parent,
	mara_value_t* parent_slot,
	mara_gc_header_t* child
)
{
	mara_value_set_gc_obj(parent_slot, child);
	mara_gc_write_barrier(ctx, parent, child);
}

#endif
