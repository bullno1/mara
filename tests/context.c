#include <munit/munit.h>
#include <mara.h>
#include <bk/default_allocator.h>


static MunitResult
create(const MunitParameter params[], void* fixture)
{
	mara_context_config_t config = {
		.allocator = bk_default_allocator
	};

	mara_context_t* ctx = mara_create_context(&config);
	mara_destroy_context(ctx);

    return MUNIT_OK;
}


static MunitTest tests[] = {
    {
        .name = "/create",
        .test = create,
    },
    { .test = NULL }
};

MunitSuite context = {
    .prefix = "/context",
    .tests = tests
};
