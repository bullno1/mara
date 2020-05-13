#ifndef MARA_INTERNAL_H
#define MARA_INTERNAL_H

#include <mara.h>
#include <ugc/ugc.h>
#include <bk/allocator.h>

#if INTPTR_MAX == INT32_MAX
#	define MARA_HASH_BITS 32
#elif INTPTR_MAX == INT64_MAX
#	define MARA_HASH_BITS 64
#else
#	define MARA_HASH_BITS 32
#endif

#define MARA_PP_CONCAT(A, B) MARA_PP_CONCAT2(A, B)
#define MARA_PP_CONCAT2(A, B) MARA_PP_CONCAT3(A, B)
#define MARA_PP_CONCAT3(A, B) A##B

#define MARA_HASH MARA_PP_CONCAT(XXH, MARA_HASH_BITS)
#define MARA_HASH_TYPE MARA_PP_CONCAT(MARA_PP_CONCAT(uint, MARA_HASH_BITS), _t)

#define MARA_ASSERT(ctx, condition, message) \
	do { \
		if(!(condition)) { ctx->config.panic_handler(ctx, message); } \
	} while(0);

#define MARA_GC_OBJ_TYPE(X) \
	X(MARA_GC_STRING) \
	X(MARA_GC_SYMBOL) \
	X(MARA_GC_LIST) \
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
	X(MARA_VAL_STRING) \
	X(MARA_VAL_SYMBOL) \
	X(MARA_VAL_LIST) \
	X(MARA_VAL_THREAD) \
	X(MARA_VAL_FUNCTION) \
	X(MARA_VAL_HANDLE)

BK_ENUM(mara_value_type_t, MARA_VAL)

#if MARA_NANBOX
#include "value_nanbox.h"
#else
#include "value_union.h"
#endif

typedef uint16_t mara_source_addr_t;
typedef MARA_HASH_TYPE mara_hash_t;
typedef struct mara_gc_header_s mara_gc_header_t;
typedef struct mara_string_s mara_string_t;
typedef struct mara_symbol_s mara_symbol_t;
typedef struct mara_list_s mara_list_t;
typedef struct mara_function_closure_s mara_function_closure_t;
typedef struct mara_function_prototype_s mara_function_prototype_t;
typedef struct mara_upvalue_s mara_upvalue_t;
typedef struct mara_source_coord_s mara_source_coord_t;
typedef struct mara_source_range_s mara_source_range_t;
typedef struct mara_strpool_s mara_strpool_t;
typedef struct mara_stack_frame_s mara_stack_frame_t;
typedef uint32_t mara_instruction_t;
typedef void(*mara_gc_visit_fn_t)(mara_context_t* ctx, mara_gc_header_t* header);

struct mara_source_coord_s
{
	mara_source_addr_t line;
	mara_source_addr_t column;
};

struct mara_source_range_s
{
	mara_source_coord_t start;
	mara_source_coord_t end;
};

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

	mara_strpool_t symtab;
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

struct mara_function_prototype_s
{
	mara_gc_header_t gc_header;

	unsigned is_vararg: 1;
	unsigned num_args: 7;
	unsigned num_upvalues: 8;
	unsigned num_instructions: 16;

	mara_instruction_t* instructions;
	mara_source_range_t* locations;
	mara_string_t* file;
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

	BK_FLEXIBLE_ARRAY_MEMBER(mara_upvalue_t, upvalues);
};

struct mara_stack_frame_s
{
	mara_value_t* base_pointer;
	mara_instruction_t* instruction_pointer;
	mara_function_closure_t* closure;

	uint8_t num_args;
};

struct mara_thread_s
{
	mara_gc_header_t gc_header;
	mara_thread_config_t config;

	mara_value_t* operand_stack;
	mara_stack_frame_t* call_stack;

	mara_stack_frame_t* frame_pointer;
};


void
mara_gc_mark(mara_context_t* ctx, mara_gc_header_t* header);

bool
mara_value_type_check(mara_value_t value, mara_value_type_t type);

mara_gc_header_t*
mara_value_as_ptr(mara_value_t value);

mara_number_t
mara_value_as_number(mara_value_t value);

bool
mara_value_as_bool(mara_value_t value);

void
mara_value_set_null(mara_value_t* value);

void
mara_value_set_number(mara_value_t* value, mara_number_t num);

void
mara_value_set_bool(mara_value_t* value, bool boolean);

void
mara_value_set_ptr(
	mara_value_t* value, mara_value_type_t type, mara_gc_header_t* ptr
);


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

#endif
