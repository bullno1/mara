#include "common.h"

static mara_fixture_t fixture;

TEST_SETUP(parser) {
	setup_mara_fixture(&fixture);
}

TEST_TEARDOWN(parser) {
	teardown_mara_fixture(&fixture);
}

TEST(parser, basic) {
	mara_exec_ctx_t* ctx = fixture.ctx;

	mara_str_t filename = MARA_INLINE_SOURCE;
	mara_str_t input = mara_str_from_literal(
		"test 1.2 ( -3_000 \"str\\n\" ) \n"
		" \"Hello\\t\"\r\n"
		"    \twhat? \r"
		"69"
	);

	mara_str_reader_t str_reader;
	mara_list_t* result;
	MARA_ASSERT_NO_ERROR(ctx, mara_parse(
		ctx,
		mara_get_local_zone(ctx),
		(mara_parse_options_t){ .filename = filename },
		mara_init_str_reader(&str_reader, input),
		&result
	));

	mara_index_t len;
	ASSERT_EQ(mara_list_len(ctx, result), 6);

	// test
	mara_value_t value = mara_list_get(ctx, result, 0);
	ASSERT_EQ(mara_value_type(value, NULL), MARA_VAL_SYM);
	mara_str_t str;
	MARA_ASSERT_NO_ERROR(ctx, mara_value_to_str(ctx, value, &str));
	MARA_ASSERT_STR_EQ(str, mara_str_from_literal("test"));

	// 1.2
	value = mara_list_get(ctx, result, 1);
	ASSERT_EQ(mara_value_type(value, NULL), MARA_VAL_REAL);
	double real;
	MARA_ASSERT_NO_ERROR(ctx, mara_value_to_real(ctx, value, &real));
	ASSERT_DOUBLE_EQ(real, 1.2);

	// ( ... )
	value = mara_list_get(ctx, result, 2);
	ASSERT_EQ(mara_value_type(value, NULL), MARA_VAL_LIST);
	mara_list_t* list;
	MARA_ASSERT_NO_ERROR(ctx, mara_value_to_list(ctx, value, &list));
	len = mara_list_len(ctx, list);
	ASSERT_EQ(len, 2);
	{
		mara_value_t elem = mara_list_get(ctx, list, 0);
		ASSERT_EQ(mara_value_type(elem, NULL), MARA_VAL_INT);
		mara_index_t integer;
		MARA_ASSERT_NO_ERROR(ctx, mara_value_to_int(ctx, elem, &integer));
		ASSERT_EQ(integer, -3000);

		elem = mara_list_get(ctx, list, 1);
		ASSERT_EQ(mara_value_type(elem, NULL), MARA_VAL_STR);
		MARA_ASSERT_NO_ERROR(ctx, mara_value_to_str(ctx, elem, &str));
		MARA_ASSERT_STR_EQ(str, mara_str_from_literal("str\n"));
	}

	// "Hello\t"
	value = mara_list_get(ctx, result, 3);
	ASSERT_EQ(mara_value_type(value, NULL), MARA_VAL_STR);
	MARA_ASSERT_NO_ERROR(ctx, mara_value_to_str(ctx, value, &str));
	MARA_ASSERT_STR_EQ(str, mara_str_from_literal("Hello\t"));

	// "what?"
	value = mara_list_get(ctx, result, 4);
	ASSERT_EQ(mara_value_type(value, NULL), MARA_VAL_SYM);
	MARA_ASSERT_NO_ERROR(ctx, mara_value_to_str(ctx, value, &str));
	MARA_ASSERT_STR_EQ(str, mara_str_from_literal("what?"));

	// 69
	value = mara_list_get(ctx, result, 5);
	ASSERT_EQ(mara_value_type(value, NULL), MARA_VAL_INT);
	mara_index_t integer;
	MARA_ASSERT_NO_ERROR(ctx, mara_value_to_int(ctx, value, &integer));
	ASSERT_EQ(integer, 69);
}

TEST(parser, nested) {
	mara_exec_ctx_t* ctx = fixture.ctx;

	mara_str_t filename = MARA_INLINE_SOURCE;
	mara_str_t input = mara_str_from_literal(
		"(1 2 3 (4 5 6))"
	);

	mara_str_reader_t str_reader;
	mara_list_t* result;
	MARA_ASSERT_NO_ERROR(ctx, mara_parse(
		ctx,
		mara_get_local_zone(ctx),
		(mara_parse_options_t){ .filename = filename },
		mara_init_str_reader(&str_reader, input),
		&result
	));
}
