#include <munit/munit.h>
#include <mara.h>
#include "helpers.h"


static void*
setup(const MunitParameter params[], void* user_data)
{
	mara_context_config_t config = mara_default_context_config();
	return mara_create_context(&config);
}

static void
tear_down(void* fixture)
{
	mara_destroy_context(fixture);
}

static MunitResult
push_pop(const MunitParameter params[], void* fixture)
{
	mara_context_t* ctx = fixture;

	munit_assert_int32(0, ==, mara_stack_len(ctx));

	mara_push_string(ctx, mara_string_ref("hello"));
	munit_assert_true(mara_is_string(ctx, 0));
	munit_assert_true(mara_is_string(ctx, -1));

	mara_index_t stack_len = mara_stack_len(ctx);
	munit_assert_int32(1, ==, stack_len);

	mara_make_symbol(ctx);
	munit_assert_false(mara_is_string(ctx, -1));
	munit_assert_true(mara_is_symbol(ctx, -1));
	munit_assert_int32(1, ==, mara_stack_len(ctx));

	mara_push_null(ctx);
	munit_assert_true(mara_is_null(ctx, -1));
	munit_assert_int32(2, ==, mara_stack_len(ctx));

	mara_push_number(ctx, 3);
	munit_assert_true(mara_is_number(ctx, -1));
	munit_assert_true(mara_is_null(ctx, 1));
	munit_assert_int32(3, ==, mara_stack_len(ctx));

	mara_dup(ctx, -2);
	munit_assert_true(mara_is_null(ctx, -1));
	munit_assert_int32(4, ==, mara_stack_len(ctx));

	munit_assert_true(mara_is_symbol(ctx, 0));
	mara_replace(ctx, 0);
	munit_assert_true(mara_is_null(ctx, 0));
	munit_assert_int32(3, ==, mara_stack_len(ctx));

	mara_restore_stack(ctx, stack_len);
	munit_assert_int32(1, ==, mara_stack_len(ctx));
	munit_assert_true(mara_is_null(ctx, 0));
	munit_assert_true(mara_is_null(ctx, -1));

	return MUNIT_OK;
}


static MunitTest tests[] = {
	{
		.name = "/push_pop",
		.test = push_pop,
		.setup = setup,
		.tear_down = tear_down,
	},
    { .test = NULL }
};

MunitSuite stack = {
    .prefix = "/stack",
    .tests = tests
};
