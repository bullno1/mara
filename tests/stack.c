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
	mara_is_string(ctx, 0);
	munit_assert_int32(1, ==, mara_stack_len(ctx));

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
