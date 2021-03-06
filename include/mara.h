#ifndef MARA_H
#define MARA_H

#include <stddef.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <bk/macro.h>

// Config

#ifndef MARA_NUMBER_TYPE
#define MARA_NUMBER_TYPE double
#define MARA_NUMBER_FMT "%f"
#define MARA_STR_TO_NUMBER strtod
#endif

#ifndef MARA_INDEX_TYPE
#define MARA_INDEX_TYPE int32_t
#endif

#ifndef MARA_NANBOX
#	if defined(__STDC_IEC_559__) && MARA_NUMBER_TYPE == double
#		define MARA_NANBOX 1
#	else
#		define MARA_NANBOX 0
#	endif
#endif

#ifndef MARA_HASH_INITIAL_CAPACITY
#define MARA_HASH_INITIAL_CAPACITY 4
#endif

#ifndef MARA_HASH_LOAD_FACTOR
#define MARA_HASH_LOAD_FACTOR 0.9
#endif

// Export

#if MARA_DYNAMIC == 1
#	if MARA_BUILD == 1
#		define MARA_DECL BK_EXTERN BK_DYNAMIC_EXPORT
#	else
#		define MARA_DECL BK_EXTERN BK_DYNAMIC_IMPORT
#	endif
#else
#	define MARA_DECL BK_EXTERN
#endif

// Types

struct bk_allocator_s;
struct bk_file_s;

typedef MARA_NUMBER_TYPE mara_number_t;
typedef MARA_INDEX_TYPE mara_index_t;
typedef struct mara_string_ref_s mara_string_ref_t;
typedef struct mara_context_s mara_context_t;
typedef struct mara_context_config_s mara_context_config_t;
typedef struct mara_thread_config_s mara_thread_config_t;
typedef struct mara_thread_s mara_thread_t;
typedef struct mara_handle_desc_s mara_handle_desc_t;
typedef struct mara_record_decl_s mara_record_decl_t;
typedef struct mara_source_coord_s mara_source_coord_t;
typedef struct mara_source_range_s mara_source_range_t;
typedef struct mara_call_s mara_call_t;
typedef uint32_t mara_source_addr_t;
typedef void(*mara_panic_fn_t)(
	mara_context_t* ctx,
	const char* message,
	const char* file,
	unsigned int line
);
typedef void(*mara_finalizer_fn_t)(mara_context_t* ctx, void* obj);

#define MARA_EXEC(X) \
	X(MARA_EXEC_OK) \
	X(MARA_EXEC_ERROR) \
	X(MARA_EXEC_YIELD)

BK_ENUM(mara_exec_t, MARA_EXEC)

#define MARA_THREAD(X) \
	X(MARA_STOPPED) \
	X(MARA_RUNNING) \
	X(MARA_PAUSED) \
	X(MARA_ERROR)

BK_ENUM(mara_thread_state_t, MARA_THREAD)

#define MARA_STRING_REF(STR) { .ptr = STR, .length = sizeof(STR) - 1 }
#define MARA_RECORD_ATTR_END { .ptr = 0, .length = 0 }

typedef mara_exec_t (*mara_native_fn_t)(mara_context_t* context);

struct mara_string_ref_s
{
	size_t length;
	const char* ptr;
};

struct mara_thread_config_s
{
	size_t stack_size;
	mara_string_ref_t name;
};

struct mara_context_config_s
{
	struct bk_allocator_s* allocator;
	mara_panic_fn_t panic_handler;
	mara_thread_config_t main_thread_config;
};

struct mara_call_s
{
	mara_index_t num_args;
};

struct mara_handle_desc_s
{
	mara_string_ref_t type_name;
	mara_finalizer_fn_t finalizer;
};

struct mara_record_decl_s
{
	mara_string_ref_t name;
	mara_string_ref_t* attributes;
};

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

// Context

MARA_DECL mara_context_t*
mara_create_context(const mara_context_config_t* config);

MARA_DECL void
mara_destroy_context(mara_context_t* ctx);

// Thread

MARA_DECL bool
mara_is_thread(mara_context_t* context, mara_index_t index);

MARA_DECL void
mara_new_thread(mara_context_t* context, const mara_thread_config_t* config);

MARA_DECL mara_exec_t
mara_resume_thread(mara_context_t* context, mara_index_t thread);

MARA_DECL mara_thread_state_t
mara_thread_state(mara_context_t* context, mara_index_t thread);

// Stack

MARA_DECL mara_index_t
mara_stack_len(mara_context_t* ctx);

MARA_DECL void
mara_restore_stack(mara_context_t* ctx, mara_index_t n);

MARA_DECL bool
mara_check_stack(mara_context_t* ctx, mara_index_t n);

MARA_DECL void
mara_push_null(mara_context_t* ctx);

MARA_DECL void
mara_push_number(mara_context_t* ctx, mara_number_t num);

MARA_DECL void
mara_push_string(mara_context_t* ctx, mara_string_ref_t str);

MARA_DECL void
mara_push_stringfv(mara_context_t* ctx, const char* fmt, va_list arg);

MARA_DECL void
mara_make_symbol(mara_context_t* ctx);

MARA_DECL void
mara_dup(mara_context_t* ctx, mara_index_t index);

MARA_DECL mara_index_t
mara_upvalue_index(mara_context_t* ctx, mara_index_t upvalue);

MARA_DECL void
mara_pop(mara_context_t* ctx, mara_index_t n);

MARA_DECL void
mara_replace(mara_context_t* ctx, mara_index_t index);

// Primitive type

MARA_DECL bool
mara_is_null(mara_context_t* ctx, mara_index_t index);

MARA_DECL bool
mara_is_bool(mara_context_t* ctx, mara_index_t index);

MARA_DECL bool
mara_is_string(mara_context_t* ctx, mara_index_t index);

MARA_DECL bool
mara_is_number(mara_context_t* ctx, mara_index_t index);

MARA_DECL bool
mara_is_symbol(mara_context_t* ctx, mara_index_t index);

MARA_DECL mara_string_ref_t
mara_as_string(mara_context_t* ctx, mara_index_t index);

MARA_DECL mara_number_t
mara_as_number(mara_context_t* ctx, mara_index_t index);

MARA_DECL bool
mara_as_bool(mara_context_t* ctx, mara_index_t index);

// Userdata

void
mara_set_context_data(
	mara_context_t* ctx, const void* handle, mara_index_t index
);

void
mara_get_context_data(mara_context_t* ctx, const void* handle);

// Record

MARA_DECL void
mara_declare_record(mara_context_t* ctx, const mara_record_decl_t* decl);

MARA_DECL void
mara_new_record(mara_context_t* ctx, const mara_record_decl_t* decl);

MARA_DECL const mara_record_decl_t*
mara_get_record_decl(mara_context_t* ctx, mara_index_t index);

MARA_DECL mara_index_t
mara_get_attribute_index(
	mara_context_t* ctx,
	const mara_record_decl_t* decl,
	mara_index_t attribute_name
);

MARA_DECL void
mara_get_attribute_name(
	mara_context_t* ctx,
	const mara_record_decl_t* decl,
	mara_index_t attribute_index
);

MARA_DECL void
mara_record_get(
	mara_context_t* ctx,
	mara_index_t record,
	mara_index_t attribute_index
);

MARA_DECL void
mara_record_set(
	mara_context_t* ctx,
	mara_index_t record,
	mara_index_t attribute_index
);

// List

MARA_DECL bool
mara_is_list(mara_context_t* ctx, mara_index_t index);

MARA_DECL void
mara_new_list(mara_context_t* ctx, mara_index_t capacity);

MARA_DECL void
mara_list_append(mara_context_t* ctx, mara_index_t list);

MARA_DECL void
mara_list_insert(mara_context_t* ctx, mara_index_t list);

MARA_DECL void
mara_list_get(mara_context_t* ctx, mara_index_t list);

MARA_DECL void
mara_list_set(mara_context_t* ctx, mara_index_t list);

MARA_DECL void
mara_list_delete(mara_context_t* ctx, mara_index_t list);

// Native object

MARA_DECL void*
mara_as_handle(
	mara_context_t* ctx, mara_index_t index, mara_handle_desc_t* desc
);

MARA_DECL void
mara_push_handle(
	mara_context_t* ctx, void* handle, mara_handle_desc_t* desc
);

// Function

MARA_DECL void
mara_push_native_function(
	mara_context_t* ctx,
	mara_native_fn_t fn,
	mara_index_t num_upvalues
);

MARA_DECL mara_exec_t
mara_exec(mara_context_t* ctx, const mara_call_t* call);

// Utils

static inline mara_string_ref_t
mara_string_ref(const char* string)
{
	return (mara_string_ref_t) {
		.ptr = string,
		.length = strlen(string),
	};
}

static inline bool
mara_string_ref_equal(mara_string_ref_t lhs, mara_string_ref_t rhs)
{
	return lhs.length == rhs.length
		&& memcmp(lhs.ptr, rhs.ptr, lhs.length) == 0;
}

static inline void
mara_push_stringf(mara_context_t* ctx, const char* fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	mara_push_stringfv(ctx, fmt, args);
	va_end(args);
}

static inline void
mara_push_symbol(mara_context_t* ctx, mara_string_ref_t string)
{
	mara_push_string(ctx, string);
	mara_make_symbol(ctx);
}

#endif
