#ifndef MARA_BIND_H
#define MARA_BIND_H

#include <mara.h>
#include <mara/utils.h>

#define MARA_WRAP(VALUE) \
	_Generic((VALUE), \
		mara_index_t: mara_value_from_int, \
		double: mara_value_from_real, \
		bool: mara_value_from_bool, \
		mara_list_t*: mara_value_from_list, \
		mara_map_t*: mara_value_from_map, \
		mara_fn_t*: mara_value_from_fn, \
		mara_value_t: mara_wrap_identity \
	)((VALUE))

#define MARA_FUNCTION(function_name) \
	mara_error_t* function_name( \
		mara_exec_ctx_t* ctx, \
		mara_index_t argc, \
		const mara_value_t* argv, \
		mara_value_t userdata, \
		mara_value_t* result \
	) \

#define MARA_FN_CHECK_ARITY(arity) \
	do { \
		if (argc < arity) { \
			return mara_errorf( \
				ctx, mara_str_from_literal("core/wrong-arity"), \
				"Function expects %d arguments, got %d", \
				mara_nil(), \
				arity, argc \
			); \
		} \
	} while (0)

#define MARA_FN_ARG(type, local_var, index) \
	type local_var; \
	MARA_FN_BIND_ARG(local_var, index);

#define MARA_FN_BIND_ARG(local_var, index) \
	do { \
		mara_error_t* bind_error = MARA_UNWRAPPER(local_var)(ctx, argv[index], &local_var); \
		if (bind_error != NULL) { \
			bind_error->extra = mara_value_from_int(index); \
			return bind_error; \
		} \
	} while (0)

#define MARA_RETURN(VALUE) \
	do { \
		*result = MARA_WRAP(VALUE); \
		return NULL; \
	} while (0)

#define MARA_RETURN_BOOL(VALUE) MARA_RETURN(mara_value_from_bool(VALUE))

#define MARA_UNWRAPPER(X) \
	_Generic((X), \
		mara_index_t: mara_value_to_int, \
		bool: mara_value_to_bool, \
		double: mara_value_to_real, \
		mara_str_t: mara_value_to_str, \
		mara_list_t*: mara_value_to_list, \
		mara_map_t*: mara_value_to_map, \
		mara_fn_t*: mara_value_to_fn, \
		mara_value_t: mara_unwrap_identity \
	)

#define MARA_EXPORT_FN(name, fn, userdata) \
	mara_export( \
		ctx, \
		mara_str_from_literal(MARA_BIND_STRINGIFY(name)), \
		mara_value_from_fn(mara_new_fn(ctx, mara_get_local_zone(ctx), fn, userdata)) \
	)

#define MARA_BIND_STRINGIFY(X) MARA_BIND_STRINGIFY2(X)
#define MARA_BIND_STRINGIFY2(X) #X

static inline mara_value_t
mara_wrap_identity(mara_value_t value) {
	return value;
}

static inline mara_error_t*
mara_unwrap_identity(mara_exec_ctx_t* ctx, mara_value_t value, mara_value_t* result) {
	(void)ctx;
	*result = value;
	return NULL;
}

#endif
