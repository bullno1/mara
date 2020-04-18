#include <munit/munit.h>
#include <mara.h>
#include <bk/default_allocator.h>


static MunitResult
create(const MunitParameter params[], void* fixture)
{
	mara_ctx_config_t config = {
		.allocator = bk_default_allocator
	};

	mara_ctx_t* ctx = mara_create_ctx(&config);
	mara_destroy_ctx(ctx);

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
