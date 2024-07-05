#include "internal.h"
#include <mara/bind.h>

#define MARA_BIN_OP(X) \
	X(mara_intrin_lt, <) \
	X(mara_intrin_lte, <=) \
	X(mara_intrin_gt, >) \
	X(mara_intrin_gte, >=)

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

MARA_PRIVATE MARA_FUNCTION(mara_intrin_plus) {
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

MARA_PRIVATE MARA_FUNCTION(mara_intrin_neg) {
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

MARA_PRIVATE MARA_FUNCTION(mara_intrin_sub) {
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

MARA_PRIVATE MARA_FUNCTION(mara_intrin_minus) {
	mara_add_native_debug_info(ctx);
	MARA_FN_CHECK_ARITY(1);

	if (argc == 1) {
		return mara_intrin_neg(ctx, argc, argv, userdata, result);
	} else {
		return mara_intrin_sub(ctx, argc, argv, userdata, result);
	}
}

MARA_PRIVATE MARA_FUNCTION(mara_intrin_make_list) {
	(void)userdata;
	mara_add_native_debug_info(ctx);

	mara_list_t* list = mara_new_list(ctx, mara_get_local_zone(ctx), argc);
	for (mara_index_t i = 0; i < argc; ++i) {
		mara_list_push(ctx, list, argv[0]);
	}

	MARA_RETURN(list);
}

MARA_PRIVATE MARA_FUNCTION(mara_intrin_put) {
	(void)userdata;
	(void)argc;
	mara_add_native_debug_info(ctx);

	MARA_FN_ARG(mara_value_t, container, 0);
	MARA_FN_ARG(mara_value_t, value, 2);

	if (mara_value_is_list(container)) {
		mara_list_t* list;
		mara_assert_no_error(mara_value_to_list(ctx, container, &list));

		MARA_FN_ARG(mara_index_t, index, 1);

		MARA_RETURN(mara_list_set(ctx, list, index, value));
	} else if (mara_value_is_map(container)) {
		mara_map_t* map;
		mara_assert_no_error(mara_value_to_map(ctx, container, &map));

		MARA_FN_ARG(mara_value_t, key, 1);

		MARA_RETURN(mara_map_set(ctx, map, key, value));
	} else {
		return mara_errorf(
			ctx,
			mara_str_from_literal("core/unexpected-type"),
			"Expecting map or list",
			mara_value_from_int(0)
		);
	}
}

MARA_PRIVATE MARA_FUNCTION(mara_intrin_get) {
	(void)userdata;
	(void)argc;
	mara_add_native_debug_info(ctx);

	MARA_FN_ARG(mara_value_t, container, 0);

	if (mara_value_is_list(container)) {
		mara_list_t* list;
		mara_assert_no_error(mara_value_to_list(ctx, container, &list));

		MARA_FN_ARG(mara_index_t, index, 1);
		MARA_RETURN(mara_list_get(ctx, list, index));
	} else if (mara_value_is_map(container)) {
		mara_map_t* map;
		mara_assert_no_error(mara_value_to_map(ctx, container, &map));

		MARA_FN_ARG(mara_value_t, key, 1);
		MARA_RETURN(mara_map_get(ctx, map, key));
	} else {
		return mara_errorf(
			ctx,
			mara_str_from_literal("core/unexpected-type"),
			"Expecting map or list",
			mara_value_from_int(0)
		);
	}
}
