#include <munit/munit.h>
#include <mara.h>
#include <bk/default_allocator.h>
#include "helpers.h"
#include "../src/internal.h"
#include "../src/strpool.h"


static void*
setup(const MunitParameter params[], void* user_data) {
	mara_context_config_t config = {
		.allocator = bk_default_allocator
	};

	return mara_create_context(&config);
}

static void
tear_down(void* fixture) {
	mara_destroy_context(fixture);
}


static MunitResult
test(const MunitParameter params[], void* fixture)
{
	mara_context_t* ctx = fixture;

	mara_string_t* foo = mara_strpool_alloc(ctx, &ctx->symtab, mara_string_ref("foo"));
	mara_string_t* bar = mara_strpool_alloc(ctx, &ctx->symtab, mara_string_ref("bar"));
	mara_string_t* foo2 = mara_strpool_alloc(ctx, &ctx->symtab, mara_string_ref("foo"));

	munit_assert_ptr_equal(foo, foo2);
	munit_assert_ptr_not_equal(foo, bar);

	mara_string_ref_t pooled_foo_ref = {
		.ptr = foo->data,
		.length = foo->length,
	};
	mara_string_ref_t pooled_bar_ref = {
		.ptr = bar->data,
		.length = bar->length,
	};

	mara_assert_string_ref_equal(mara_string_ref("foo"), pooled_foo_ref);
	mara_assert_string_ref_equal(mara_string_ref("bar"), pooled_bar_ref);

	// Force resize
	mara_string_t* quux = mara_strpool_alloc(ctx, &ctx->symtab, mara_string_ref("quux"));
	mara_string_t* quuux = mara_strpool_alloc(ctx, &ctx->symtab, mara_string_ref("quuux"));
	mara_string_t* quuuux = mara_strpool_alloc(ctx, &ctx->symtab, mara_string_ref("quuuux"));
	mara_string_t* quux2 = mara_strpool_alloc(ctx, &ctx->symtab, mara_string_ref("quux"));
	mara_string_t* quuuux2 = mara_strpool_alloc(ctx, &ctx->symtab, mara_string_ref("quuuux"));

	foo = mara_strpool_alloc(ctx, &ctx->symtab, mara_string_ref("foo"));
	bar = mara_strpool_alloc(ctx, &ctx->symtab, mara_string_ref("bar"));
	foo2 = mara_strpool_alloc(ctx, &ctx->symtab, mara_string_ref("foo"));

	munit_assert_ptr_equal(foo, foo2);
	munit_assert_ptr_equal(quux, quux2);
	munit_assert_ptr_equal(quuuux, quuuux2);
	munit_assert_ptr_not_equal(foo, bar);
	munit_assert_ptr_not_equal(foo, quux);

	pooled_foo_ref = (mara_string_ref_t){
		.ptr = foo->data,
		.length = foo->length,
	};
	pooled_bar_ref = (mara_string_ref_t){
		.ptr = bar->data,
		.length = bar->length,
	};

	mara_assert_string_ref_equal(mara_string_ref("foo"), pooled_foo_ref);
	mara_assert_string_ref_equal(mara_string_ref("bar"), pooled_bar_ref);


	return MUNIT_OK;
}


static MunitTest tests[] = {
	{
		.name = "/test",
		.test = test,
		.setup = setup,
		.tear_down = tear_down,
	},
	{ .test = NULL }
};

MunitSuite strpool = {
	.prefix = "/strpool",
	.tests = tests
};
