#include <munit/munit.h>
#include <mara.h>
#include <bk/default_allocator.h>


static void*
test_setup(const MunitParameter params[], void* user_data) {
	mara_ctx_config_t config = {
		.allocator = bk_default_allocator
	};

	return mara_create_ctx(&config);
}

static void
test_tear_down(void* fixture) {
	/*mara_destroy_ctx(fixture);*/
}


static MunitResult
test(const MunitParameter params[], void* fixture)
{
    return MUNIT_OK;
}


static MunitTest tests[] = {
    {
        .name = "/test",
        .test = test,
    },
    { .test = NULL }
};

MunitSuite strpool = {
    .prefix = "/strpool",
    .tests = tests
};
