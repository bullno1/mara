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
	mara_value_t result;
	MARA_ASSERT_NO_ERROR(ctx, mara_parse_all(
		ctx,
		mara_get_local_zone(ctx),
		filename,
		mara_init_str_reader(&str_reader, input),
		&result
	));

	mara_index_t len;
	MARA_ASSERT_NO_ERROR(ctx, mara_list_len(ctx, result, &len));
	ASSERT_EQ(len, 6);

	// test
	mara_value_t value;
	MARA_ASSERT_NO_ERROR(ctx, mara_list_get(ctx, result, 0, &value));
	ASSERT_EQ(mara_value_type(value, NULL), MARA_VAL_SYMBOL);
	mara_str_t str;
	MARA_ASSERT_NO_ERROR(ctx, mara_value_to_str(ctx, value, &str));
	MARA_ASSERT_STR_EQ(str, mara_str_from_literal("test"));

	// 1.2
	MARA_ASSERT_NO_ERROR(ctx, mara_list_get(ctx, result, 1, &value));
	ASSERT_EQ(mara_value_type(value, NULL), MARA_VAL_REAL);
	double real;
	MARA_ASSERT_NO_ERROR(ctx, mara_value_to_real(ctx, value, &real));
	ASSERT_DOUBLE_EQ(real, 1.2);

	// ( ... )
	MARA_ASSERT_NO_ERROR(ctx, mara_list_get(ctx, result, 2, &value));
	ASSERT_EQ(mara_value_type(value, NULL), MARA_VAL_LIST);
	MARA_ASSERT_NO_ERROR(ctx, mara_list_len(ctx, value, &len));
	ASSERT_EQ(len, 2);
	{
		mara_value_t elem;
		MARA_ASSERT_NO_ERROR(ctx, mara_list_get(ctx, value, 0, &elem));
		ASSERT_EQ(mara_value_type(elem, NULL), MARA_VAL_INT);
		mara_index_t integer;
		MARA_ASSERT_NO_ERROR(ctx, mara_value_to_int(ctx, elem, &integer));
		ASSERT_EQ(integer, -3000);

		MARA_ASSERT_NO_ERROR(ctx, mara_list_get(ctx, value, 1, &elem));
		ASSERT_EQ(mara_value_type(elem, NULL), MARA_VAL_STRING);
		MARA_ASSERT_NO_ERROR(ctx, mara_value_to_str(ctx, elem, &str));
		MARA_ASSERT_STR_EQ(str, mara_str_from_literal("str\n"));
	}

	// "Hello\t"
	MARA_ASSERT_NO_ERROR(ctx, mara_list_get(ctx, result, 3, &value));
	ASSERT_EQ(mara_value_type(value, NULL), MARA_VAL_STRING);
	MARA_ASSERT_NO_ERROR(ctx, mara_value_to_str(ctx, value, &str));
	MARA_ASSERT_STR_EQ(str, mara_str_from_literal("Hello\t"));

	// "what?"
	MARA_ASSERT_NO_ERROR(ctx, mara_list_get(ctx, result, 4, &value));
	ASSERT_EQ(mara_value_type(value, NULL), MARA_VAL_SYMBOL);
	MARA_ASSERT_NO_ERROR(ctx, mara_value_to_str(ctx, value, &str));
	MARA_ASSERT_STR_EQ(str, mara_str_from_literal("what?"));

	// 69
	MARA_ASSERT_NO_ERROR(ctx, mara_list_get(ctx, result, 5, &value));
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
	mara_value_t result;
	MARA_ASSERT_NO_ERROR(ctx, mara_parse_all(
		ctx,
		mara_get_local_zone(ctx),
		filename,
		mara_init_str_reader(&str_reader, input),
		&result
	));
}
