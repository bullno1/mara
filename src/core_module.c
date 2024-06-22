#include "internal.h"
#include <mara/bind.h>

// Arithmetic

#define MARA_BIN_OP(X) \
	X(mara_core_lt, <) \
	X(mara_core_lte, <=) \
	X(mara_core_gt, >) \
	X(mara_core_gte, >=)

#define MARA_DEFINE_BIN_OP(NAME, OP) \
	MARA_PRIVATE MARA_FUNCTION(NAME) { \
		(void)userdata; \
		mara_add_native_debug_info(ctx); \
		MARA_FN_CHECK_ARITY(2); \
		if (mara_value_is_int(argv[0])) { \
			MARA_FN_ARG(mara_index_t, lhs, 0); \
			MARA_FN_ARG(mara_index_t, rhs, 1); \
			MARA_RETURN_BOOL(lhs OP rhs); \
		} else if (mara_value_is_real(argv[0])) { \
			MARA_FN_ARG(mara_real_t, lhs, 0); \
			MARA_FN_ARG(mara_real_t, rhs, 1); \
			MARA_RETURN_BOOL(lhs OP rhs); \
		} else { \
			return mara_errorf( \
				ctx, \
				mara_str_from_literal("core/unexpected-type"), \
				"Expecting number", \
				mara_value_from_int(0) \
			); \
		} \
	}

MARA_BIN_OP(MARA_DEFINE_BIN_OP)

MARA_PRIVATE MARA_FUNCTION(mara_core_plus) {
	(void)userdata;
	mara_add_native_debug_info(ctx);

	if (argc == 0) {
		MARA_RETURN(0);
	}
	if (mara_value_is_int(argv[0])) {
		mara_index_t total = 0;
		for (mara_index_t i = 0; i < argc; ++i) {
			MARA_FN_ARG(mara_index_t, value, i);
			total += value;
		}
		MARA_RETURN(total);
	} else if (mara_value_is_real(argv[0])) {
		mara_real_t total = 0;
		for (mara_index_t i = 0; i < argc; ++i) {
			MARA_FN_ARG(mara_real_t, value, i);
			total += value;
		}
		MARA_RETURN(total);
	} else {
		return mara_errorf(
			ctx,
			mara_str_from_literal("core/unexpected-type"),
			"Expecting number",
			mara_value_from_int(0)
		);
	}
}

MARA_PRIVATE MARA_FUNCTION(mara_core_minus) {
	(void)userdata;
	mara_add_native_debug_info(ctx);
	MARA_FN_CHECK_ARITY(1);

	if (argc == 1) {
		if (mara_value_is_int(argv[0])) {
			MARA_FN_ARG(mara_index_t, value, 0);
			MARA_RETURN(-value);
		} else if (mara_value_is_real(argv[0])) {
			MARA_FN_ARG(mara_real_t, value, 0);
			MARA_RETURN(-value);
		} else {
			return mara_errorf(
				ctx,
				mara_str_from_literal("core/unexpected-type"),
				"Expecting number",
				mara_value_from_int(0)
			);
		}
	} else {
		if (mara_value_is_int(argv[0])) {
			MARA_FN_ARG(mara_index_t, acc, 0);
			for (mara_index_t i = 1; i < argc; ++i) {
				MARA_FN_ARG(mara_index_t, value, i);
				acc -= value;
			}
			MARA_RETURN(acc);
		} else if (mara_value_is_real(argv[0])) {
			MARA_FN_ARG(mara_real_t, acc, 0);
			for (mara_index_t i = 1; i < argc; ++i) {
				MARA_FN_ARG(mara_real_t, value, i);
				acc -= value;
			}
			MARA_RETURN(acc);
		} else {
			return mara_errorf(
				ctx,
				mara_str_from_literal("core/unexpected-type"),
				"Expecting number",
				mara_value_from_int(0)
			);
		}
	}
}

// List

MARA_PRIVATE MARA_FUNCTION(mara_core_list_new) {
	(void)userdata;
	mara_add_native_debug_info(ctx);

	mara_index_t capacity = 0;
	if (argc >= 1) {
		MARA_FN_BIND_ARG(capacity, 0);
	}
	MARA_RETURN(mara_new_list(ctx, mara_get_return_zone(ctx), capacity));
}

MARA_PRIVATE MARA_FUNCTION(mara_core_list_len) {
	(void)userdata;
	mara_add_native_debug_info(ctx);
	MARA_FN_CHECK_ARITY(1);
	MARA_FN_ARG(mara_list_t*, list, 0);

	MARA_RETURN(mara_list_len(ctx, list));
}

MARA_PRIVATE MARA_FUNCTION(mara_core_list_push) {
	(void)userdata;
	mara_add_native_debug_info(ctx);
	MARA_FN_CHECK_ARITY(2);
	MARA_FN_ARG(mara_list_t*, list, 0);
	MARA_FN_ARG(mara_value_t, value, 1);

	mara_list_push(ctx, list, value);
	MARA_RETURN(mara_nil());
}

MARA_PRIVATE MARA_FUNCTION(mara_core_list_set) {
	(void)userdata;
	mara_add_native_debug_info(ctx);
	MARA_FN_CHECK_ARITY(3);
	MARA_FN_ARG(mara_list_t*, list, 0);
	MARA_FN_ARG(mara_index_t, index, 1);
	MARA_FN_ARG(mara_value_t, value, 2);

	MARA_RETURN(mara_list_set(ctx, list, index, value));
}

MARA_PRIVATE MARA_FUNCTION(mara_core_module_entry) {
	(void)argv;
	(void)userdata;
	MARA_FN_CHECK_ARITY(3);

	MARA_EXPORT_FN(<, mara_core_lt, NULL);
	MARA_EXPORT_FN(<=, mara_core_lte, NULL);
	MARA_EXPORT_FN(>, mara_core_gt, NULL);
	MARA_EXPORT_FN(>=, mara_core_gte, NULL);

	MARA_EXPORT_FN(+, mara_core_plus, NULL);
	MARA_EXPORT_FN(-, mara_core_minus, NULL);

	MARA_EXPORT_FN(list/new, mara_core_list_new, NULL);
	MARA_EXPORT_FN(list/len, mara_core_list_len, NULL);
	MARA_EXPORT_FN(list/push, mara_core_list_push, NULL);
	MARA_EXPORT_FN(list/set, mara_core_list_set, NULL);

	MARA_RETURN(mara_value_from_bool(true));
}

MARA_PRIVATE MARA_FUNCTION(mara_load_core_module) {
	(void)ctx;
	MARA_FN_CHECK_ARITY(1);
	MARA_FN_ARG(mara_value_t, module_name, 0);

	mara_value_t core_module_name = (mara_value_t){ .internal = (uintptr_t)userdata };
	if (module_name.internal != core_module_name.internal) {
		*result = mara_nil();
		return NULL;
	}

	MARA_RETURN(mara_new_fn(ctx, mara_get_return_zone(ctx), mara_core_module_entry, NULL));
}

void
mara_add_core_module(mara_exec_ctx_t* ctx) {
	mara_fn_t* loader = mara_new_fn(
		ctx, ctx->current_zone,
		mara_load_core_module,
		(void*)(uintptr_t)mara_new_sym(ctx, mara_str_from_literal("core")).internal
	);
	mara_add_module_loader(ctx, loader);
	mara_value_t result;
	mara_import(ctx, mara_str_from_literal("core"), mara_str_from_literal("*main*"), &result);
}
