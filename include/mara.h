#ifndef MARA_H
#define MARA_H

#include <stddef.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <bk/macro.h>

#if MARA_DYNAMIC == 1
#	if MARA_BUILD == 1
#		define MARA_DECL BK_EXTERN BK_DYNAMIC_EXPORT
#	else
#		define MARA_DECL BK_EXTERN BK_DYNAMIC_IMPORT
#	endif
#else
#	define MARA_DECL BK_EXTERN
#endif

#ifndef MARA_REAL_TYPE
#define MARA_REAL_TYPE double
#define MARA_REAL_FMT "%f"
#define MARA_STR_TO_REAL strtod
#endif

#ifndef MARA_INT_TYPE
#define MARA_INT_TYPE int64_t
#define MARA_INT_FMT "%" PRId64
#define MARA_STR_TO_INT strtoll
#endif

struct bk_allocator_s;
struct bk_file_s;

typedef MARA_REAL_TYPE mara_real_t;
typedef MARA_INT_TYPE mara_int_t;
typedef struct mara_string_ref_s mara_string_ref_t;
typedef struct mara_ctx_s mara_ctx_t;
typedef struct mara_ctx_config_s mara_ctx_config_t;
typedef struct mara_thread_s mara_thread_t;
typedef struct mara_thread_config_s mara_thread_config_t;
typedef void(*mara_panic_fn_t)(mara_ctx_t* ctx, const char* message);

#define MARA_VAL(X) \
	X(MARA_VAL_NULL) \
	X(MARA_VAL_INT) \
	X(MARA_VAL_REAL) \
	X(MARA_VAL_STRING) \
	X(MARA_VAL_ATOM) \
	X(MARA_VAL_LIST) \
	X(MARA_VAL_FUNCTION)

BK_ENUM(mara_value_type_t, MARA_VAL)

#define MARA_EXEC(X) \
	X(MARA_EXEC_OK) \
	X(MARA_EXEC_ERROR) \
	X(MARA_EXEC_YIELD)

BK_ENUM(mara_exec_t, MARA_EXEC)

struct mara_string_ref_s
{
	size_t length;
	const char* ptr;
};

struct mara_ctx_config_s
{
	struct bk_allocator_s* allocator;
	mara_panic_fn_t panic_handler;
};

struct mara_thread_config_s
{
	uint16_t operand_stack_size;
	uint16_t call_stack_size;
};

// Context

MARA_DECL mara_ctx_t*
mara_create_ctx(const mara_ctx_config_t* config);

MARA_DECL void
mara_destroy_ctx(mara_ctx_t* ctx);

// Thread

MARA_DECL mara_thread_t*
mara_create_thread(const mara_thread_config_t* config);

MARA_DECL void
mara_destroy_thread(mara_ctx_t* ctx, mara_thread_t* thread);

MARA_DECL mara_thread_t*
mara_set_current_thread(mara_ctx_t* ctx, mara_thread_t* thread);

// Stack

MARA_DECL mara_int_t
mara_stack_len(mara_ctx_t* ctx);

MARA_DECL void
mara_push_null(mara_ctx_t* ctx);

MARA_DECL void
mara_push_int(mara_ctx_t* ctx, mara_int_t num);

MARA_DECL void
mara_push_real(mara_ctx_t* ctx, mara_real_t num);

MARA_DECL void
mara_push_string(mara_ctx_t* ctx, mara_string_ref_t str);

MARA_DECL void
mara_push_string_fmtv(mara_ctx_t* ctx, const char* fmt, va_list args);

MARA_DECL void
mara_make_symbol(mara_ctx_t* ctx, mara_int_t index);

MARA_DECL void
mara_dup(mara_ctx_t* ctx, mara_int_t index);

MARA_DECL void
mara_replace(mara_ctx_t* ctx, mara_int_t index);

MARA_DECL bool
mara_check_type(mara_ctx_t* ctx, mara_int_t index, mara_value_type_t type);

MARA_DECL mara_int_t
mara_obj_len(mara_ctx_t* ctx, mara_int_t index);

// Simple type

MARA_DECL mara_string_ref_t
mara_as_string(mara_ctx_t* ctx, mara_int_t index);

MARA_DECL mara_int_t
mara_as_int(mara_ctx_t* ctx, mara_int_t index);

MARA_DECL mara_real_t
mara_as_real(mara_ctx_t* ctx, mara_int_t index);

// List

MARA_DECL void
mara_list_new(mara_ctx_t* ctx, mara_int_t capacity);

MARA_DECL void
mara_list_get(mara_ctx_t* ctx, mara_int_t list);

MARA_DECL void
mara_list_set(mara_ctx_t* ctx, mara_int_t list);

MARA_DECL void
mara_list_append(mara_ctx_t* ctx, mara_int_t list);

MARA_DECL void
mara_list_insert(mara_ctx_t* ctx, mara_int_t list);

MARA_DECL void
mara_list_delete(mara_ctx_t* ctx, mara_int_t list);

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

#endif
