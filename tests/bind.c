#include "rktest.h"
#include <mara.h>
#include <mara/utils.h>
#include <mara/bind.h>
#include "common.h"

static mara_fixture_t fixture;

typedef struct {
	int num_elements;
} iterator_state_t;

TEST_SETUP(bind) {
	setup_mara_fixture(&fixture);
}

TEST_TEARDOWN(bind) {
	teardown_mara_fixture(&fixture);
}

MARA_FUNCTION(return_bool) {
	(void)ctx;
	(void)argc;
	(void)argv;
	(void)userdata;
	MARA_RETURN_BOOL(true);
}

TEST(bind, return_bool) {
	mara_exec_ctx_t* ctx = fixture.ctx;

	mara_native_fn_options_t options = { 0 };
	mara_fn_t* fn = mara_new_fn(ctx, mara_get_local_zone(ctx), return_bool, options);
	mara_value_t result;
	MARA_ASSERT_NO_ERROR(
		ctx,
		mara_call(ctx, mara_get_local_zone(ctx), fn, 0, NULL, &result)
	);
	ASSERT_TRUE(mara_value_is_bool(result));
}
