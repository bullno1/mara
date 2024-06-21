#ifndef MARA_H
#define MARA_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#ifdef MARA_SHARED
#	if defined(_WIN32)
#		ifdef MARA_BUILD
#			define MARA_API __declspec(dllexport)
#		else
#			define MARA_API __declspec(dllimport)
#		endif
#	else
#		ifdef MARA_BUILD
#			define MARA_API __attribute__((visibility ("default")))
#		else
#			define MARA_API
#		endif
#	endif
#else
#	define MARA_API
#endif

#ifdef __GNUC__
#	define MARA_PRINTF_LIKE(FMT, ARGS) __attribute__((format(printf, FMT, ARGS)))
#else
#	define MARA_PRINTF_LIKE(FMT, ARGS)
#endif

typedef struct mara_env_s mara_env_t;
typedef struct mara_exec_ctx_s mara_exec_ctx_t;
typedef struct mara_zone_s mara_zone_t;
typedef uint64_t mara_value_t;
typedef int32_t mara_index_t;

typedef struct {
	void* (*fn)(void* ptr, size_t size, void* userdata);
	void* userdata;
} mara_allocator_t;

typedef mara_index_t (*mara_read_fn_t)(void* buffer, mara_index_t size, void* userdata);
typedef struct {
	mara_read_fn_t fn;
	void* userdata;
} mara_reader_t;

typedef struct {
	mara_index_t (*fn)(const void* buffer, mara_index_t size, void* userdata);
	void* userdata;
} mara_writer_t;

typedef struct {
	mara_index_t len;
	const char* data;
} mara_str_t;

typedef struct {
	mara_index_t line;
	mara_index_t col;
	mara_index_t byte_offset;
} mara_source_pos_t;

typedef struct {
	mara_source_pos_t start;
	mara_source_pos_t end;
} mara_source_range_t;

typedef struct {
	mara_str_t filename;
	mara_source_range_t range;
} mara_source_info_t;

typedef struct {
	mara_index_t len;
	mara_source_info_t frames[];
} mara_stacktrace_t;

typedef struct {
	mara_str_t type;
	mara_str_t message;
	mara_value_t extra;
	mara_stacktrace_t* stacktrace;
} mara_error_t;

typedef struct {
	mara_error_t* (*fn)(
		mara_exec_ctx_t* ctx,
		mara_index_t argc,
		const mara_value_t* argv,
		void* userdata,
		mara_value_t* result
	);
	void* userdata;
} mara_native_fn_t;

typedef struct {
	mara_allocator_t allocator;
	size_t alloc_chunk_size;
} mara_env_options_t;

typedef struct {
	mara_index_t max_depth;
	mara_index_t max_length;
	mara_index_t indent;
} mara_print_options_t;

typedef struct {
	mara_str_t module_name;
	bool ignore_export;
} mara_module_options_t;

typedef struct {
	void* (*open)(mara_str_t filename, void* userdata);
	mara_read_fn_t read;
	void (*close)(void* handle, void* userdata);

	void* userdata;
} mara_module_fs_t;

typedef enum {
	MARA_VAL_NIL,
	MARA_VAL_INT,
	MARA_VAL_REAL,
	MARA_VAL_BOOL,
	MARA_VAL_STRING,
	MARA_VAL_SYMBOL,
	MARA_VAL_REF,
	MARA_VAL_FUNCTION,
	MARA_VAL_LIST,
	MARA_VAL_MAP,
} mara_value_type_t;

typedef struct {
	bool as_module;
	bool strip_debug_info;
} mara_compile_options_t;

#ifdef __cplusplus
extern "C" {
#endif

// Environment

MARA_API mara_env_t*
mara_create_env(mara_env_options_t options);

MARA_API void
mara_destroy_env(mara_env_t* env);

MARA_API mara_exec_ctx_t*
mara_begin(mara_env_t* env);

MARA_API void
mara_end(mara_exec_ctx_t* ctx);

// Debug

MARA_API void
mara_set_debug_info(mara_exec_ctx_t* ctx, mara_source_info_t debug_info);

MARA_API void
mara_print_value(
	mara_exec_ctx_t* ctx,
	mara_value_t value,
	mara_print_options_t options,
	mara_writer_t output
);

MARA_API void
mara_print_error(
	mara_exec_ctx_t* ctx,
	mara_error_t* error,
	mara_print_options_t options,
	mara_writer_t output
);

// Zone

MARA_API mara_zone_t*
mara_get_local_zone(mara_exec_ctx_t* ctx);

MARA_API mara_zone_t*
mara_get_return_zone(mara_exec_ctx_t* ctx);

MARA_API mara_zone_t*
mara_get_error_zone(mara_exec_ctx_t* ctx);

MARA_API mara_zone_t*
mara_get_zone_of(mara_exec_ctx_t* ctx, mara_value_t value);

MARA_API void*
mara_zone_alloc(mara_exec_ctx_t* ctx, mara_zone_t* zone, size_t size);

MARA_API void*
mara_zone_alloc_ex(mara_exec_ctx_t* ctx, mara_zone_t* zone, size_t size, size_t alignment);

// Value

MARA_API bool
mara_value_is_nil(mara_value_t value);

MARA_API bool
mara_value_is_int(mara_value_t value);

MARA_API bool
mara_value_is_real(mara_value_t value);

MARA_API bool
mara_value_is_bool(mara_value_t value);

MARA_API bool
mara_value_is_true(mara_value_t value);

MARA_API bool
mara_value_is_false(mara_value_t value);

MARA_API bool
mara_value_is_str(mara_value_t value);

MARA_API bool
mara_value_is_symbol(mara_value_t value);

MARA_API bool
mara_value_is_ref(mara_value_t value, void* tag);

MARA_API bool
mara_value_is_function(mara_value_t value);

MARA_API bool
mara_value_is_list(mara_value_t value);

MARA_API bool
mara_value_is_map(mara_value_t value);

MARA_API mara_value_type_t
mara_value_type(mara_value_t value, void** tag);

MARA_API mara_error_t*
mara_value_to_int(mara_exec_ctx_t* ctx, mara_value_t value, int* result);

MARA_API mara_error_t*
mara_value_to_real(mara_exec_ctx_t* ctx, mara_value_t value, double* result);

MARA_API mara_error_t*
mara_value_to_bool(mara_exec_ctx_t* ctx, mara_value_t value, bool* result);

MARA_API mara_error_t*
mara_value_to_str(mara_exec_ctx_t* ctx, mara_value_t value, mara_str_t* result);

MARA_API mara_error_t*
mara_value_to_ref(mara_exec_ctx_t* ctx, mara_value_t value, void* tag, void** result);

MARA_API mara_value_t
mara_nil(void);

MARA_API mara_value_t
mara_value_from_bool(bool value);

MARA_API mara_value_t
mara_value_from_int(mara_index_t value);

MARA_API mara_value_t
mara_value_from_real(double value);

MARA_API mara_value_t
mara_new_str(mara_exec_ctx_t* ctx, mara_zone_t* zone, mara_str_t value);

MARA_API mara_value_t
mara_new_strf(
	mara_exec_ctx_t* ctx,
	mara_zone_t* zone,
	const char* fmt,
	...
) MARA_PRINTF_LIKE(3, 4);

MARA_API mara_value_t
mara_new_strv(
	mara_exec_ctx_t* ctx,
	mara_zone_t* zone,
	const char* fmt,
	va_list args
);

MARA_API mara_value_t
mara_new_symbol(mara_exec_ctx_t* ctx, mara_str_t name);

MARA_API mara_value_t
mara_new_fn(mara_exec_ctx_t* ctx, mara_zone_t* zone, mara_native_fn_t fn);

MARA_API mara_value_t
mara_new_list(mara_exec_ctx_t* ctx, mara_zone_t* zone, mara_index_t initial_capacity);

MARA_API mara_value_t
mara_new_map(mara_exec_ctx_t* ctx, mara_zone_t* zone);

MARA_API mara_value_t
mara_new_ref(mara_exec_ctx_t* ctx, mara_zone_t* zone, void* tag, void* value);

MARA_API mara_error_t*
mara_copy(mara_exec_ctx_t* ctx, mara_zone_t* zone, mara_value_t value, mara_value_t* result);

// Error

MARA_API mara_error_t*
mara_errorf(
	mara_exec_ctx_t* ctx,
	mara_str_t type,
	const char* fmt,
	mara_value_t extra,
	...
) MARA_PRINTF_LIKE(3, 5);

MARA_API mara_error_t*
mara_errorv(
	mara_exec_ctx_t* ctx,
	mara_str_t type,
	const char* fmt,
	mara_value_t extra,
	va_list args
);

// List

MARA_API mara_error_t*
mara_list_len(mara_exec_ctx_t* ctx, mara_value_t list, mara_index_t* result);

MARA_API mara_error_t*
mara_list_get(mara_exec_ctx_t* ctx, mara_value_t list, mara_index_t index, mara_value_t* result);

MARA_API mara_error_t*
mara_list_set(mara_exec_ctx_t* ctx, mara_value_t list, mara_index_t index, mara_value_t value);

MARA_API mara_error_t*
mara_list_push(mara_exec_ctx_t* ctx, mara_value_t list, mara_value_t value);

MARA_API mara_error_t*
mara_list_delete(mara_exec_ctx_t* ctx, mara_value_t list, mara_index_t index);

MARA_API mara_error_t*
mara_list_quick_delete(mara_exec_ctx_t* ctx, mara_value_t list, mara_index_t index);

MARA_API mara_error_t*
mara_list_resize(mara_exec_ctx_t* ctx, mara_value_t list, mara_index_t len);

MARA_API mara_error_t*
mara_list_foreach(mara_exec_ctx_t* ctx, mara_value_t list, mara_native_fn_t fn);

// Map

MARA_API mara_error_t*
mara_map_len(mara_exec_ctx_t* ctx, mara_value_t map, mara_index_t* result);

MARA_API mara_error_t*
mara_map_set(mara_exec_ctx_t* ctx, mara_value_t map, mara_value_t key, mara_value_t value);

MARA_API mara_error_t*
mara_map_get(mara_exec_ctx_t* ctx, mara_value_t map, mara_value_t key, mara_value_t* result);

MARA_API mara_error_t*
mara_map_delete(mara_exec_ctx_t* ctx, mara_value_t map, mara_value_t key, mara_value_t* result);

MARA_API mara_error_t*
mara_map_foreach(mara_exec_ctx_t* ctx, mara_value_t map, mara_native_fn_t fn);

// Module

MARA_API mara_error_t*
mara_import(
	mara_exec_ctx_t* ctx,
	mara_str_t module_name,
	mara_str_t export_name,
	mara_value_t* result
);

MARA_API mara_error_t*
mara_init_module(
	mara_exec_ctx_t* ctx,
	mara_value_t entry_fn,
	mara_module_options_t options
);

MARA_API mara_error_t*
mara_add_module_loader(mara_exec_ctx_t* ctx, mara_value_t fn);

MARA_API void
mara_add_standard_loader(mara_exec_ctx_t* ctx, mara_module_fs_t fs);

MARA_API void
mara_reload(mara_env_t* env);

// Function

MARA_API mara_error_t*
mara_call(
	mara_exec_ctx_t* ctx,
	mara_zone_t* zone,
	mara_value_t fn,
	mara_index_t argc,
	mara_value_t* argv,
	mara_value_t* result
);

MARA_API mara_error_t*
mara_apply(
	mara_exec_ctx_t* ctx,
	mara_zone_t* zone,
	mara_value_t fn,
	mara_value_t args,
	mara_value_t* result
);

// Compile

MARA_API mara_error_t*
mara_parse_all(
	mara_exec_ctx_t* ctx,
	mara_zone_t* zone,
	mara_str_t filename,
	mara_reader_t reader,
	mara_value_t* result
);

MARA_API mara_error_t*
mara_parse_one(
	mara_exec_ctx_t* ctx,
	mara_zone_t* zone,
	mara_str_t filename,
	mara_reader_t reader,
	mara_value_t* result
);

MARA_API mara_error_t*
mara_compile(
	mara_exec_ctx_t* ctx,
	mara_zone_t* zone,
	mara_compile_options_t options,
	mara_value_t exprs,
	mara_value_t* result
);

// Serialization

MARA_API mara_error_t*
mara_load(mara_exec_ctx_t* ctx, mara_zone_t* zone, mara_reader_t reader, mara_value_t* result);

MARA_API mara_error_t*
mara_dump(mara_exec_ctx_t* ctx, mara_value_t value, mara_writer_t writer, mara_value_t* result);

#ifdef __cplusplus
}
#endif

#endif
