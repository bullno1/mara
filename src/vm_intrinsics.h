#include "internal.h"
#include <mara/bind.h>

#define MARA_BIN_OP(X) \
	X(LT, <) \
	X(LTE, <=) \
	X(GT, >) \
	X(GTE, >=)

#define MARA_DEFINE_BIN_OP(NAME, OP) \
	MARA_PRIVATE MARA_FUNCTION(mara_intrin_##NAME) { \
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

MARA_PRIVATE MARA_FUNCTION(mara_intrin_PLUS) {
	(void)userdata;
	mara_add_native_debug_info(ctx);

	if (argc == 0) {
		MARA_RETURN(0);
	} else if (mara_value_is_int(argv[0])) {
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

MARA_PRIVATE MARA_FUNCTION(mara_intrin_NEG) {
	(void)argc;
	(void)userdata;
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
}

MARA_PRIVATE MARA_FUNCTION(mara_intrin_SUB) {
	(void)userdata;
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

MARA_PRIVATE MARA_FUNCTION(mara_intrin_MINUS) {
	mara_add_native_debug_info(ctx);
	MARA_FN_CHECK_ARITY(1);

	if (argc == 1) {
		return mara_intrin_NEG(ctx, argc, argv, userdata, result);
	} else {
		return mara_intrin_SUB(ctx, argc, argv, userdata, result);
	}
}

MARA_PRIVATE MARA_FUNCTION(mara_intrin_MAKE_LIST) {
	(void)userdata;
	mara_add_native_debug_info(ctx);

	mara_list_t* list = mara_new_list(ctx, mara_get_local_zone(ctx), argc);
	for (mara_index_t i = 0; i < argc; ++i) {
		mara_list_push(ctx, list, argv[0]);
	}

	MARA_RETURN(list);
}

MARA_PRIVATE MARA_FUNCTION(mara_intrin_LIST_NEW) {
	(void)userdata;
	mara_add_native_debug_info(ctx);

	mara_index_t capacity = 0;
	if (argc >= 1) {
		MARA_FN_BIND_ARG(capacity, 0);
	}
	MARA_RETURN(mara_new_list(ctx, mara_get_return_zone(ctx), capacity));
}

MARA_PRIVATE MARA_FUNCTION(mara_intrin_LIST_LEN) {
	(void)userdata;
	mara_add_native_debug_info(ctx);
	MARA_FN_CHECK_ARITY(1);
	MARA_FN_ARG(mara_list_t*, list, 0);

	MARA_RETURN(mara_list_len(ctx, list));
}

MARA_PRIVATE MARA_FUNCTION(mara_intrin_LIST_PUSH) {
	(void)userdata;
	mara_add_native_debug_info(ctx);
	MARA_FN_CHECK_ARITY(2);
	MARA_FN_ARG(mara_list_t*, list, 0);
	MARA_FN_ARG(mara_value_t, value, 1);

	mara_list_push(ctx, list, value);
	MARA_RETURN(mara_nil());
}

MARA_PRIVATE MARA_FUNCTION(mara_intrin_LIST_SET) {
	(void)userdata;
	(void)argc;
	mara_add_native_debug_info(ctx);

	MARA_FN_ARG(mara_list_t*, list, 0);
	MARA_FN_ARG(mara_index_t, index, 1);
	MARA_FN_ARG(mara_value_t, value, 2);

	MARA_RETURN(mara_list_set(ctx, list, index, value));
}

MARA_PRIVATE MARA_FUNCTION(mara_intrin_LIST_GET) {
	(void)userdata;
	(void)argc;
	mara_add_native_debug_info(ctx);

	MARA_FN_ARG(mara_list_t*, list, 0);
	MARA_FN_ARG(mara_index_t, index, 1);

	MARA_RETURN(mara_list_get(ctx, list, index));
}
