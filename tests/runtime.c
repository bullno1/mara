#include "rktest.h"
#include <mara.h>
#include <mara/utils.h>

static struct {
	mara_env_t* env;
} fixture;

TEST_SETUP(runtime) {
	fixture.env = mara_create_env((mara_env_options_t){ 0 });
}

TEST_TEARDOWN(runtime) {
	mara_destroy_env(fixture.env);
}

static inline void
test_symbol(mara_exec_ctx_t* ctx, void* userdata) {
	(void)userdata;

	char cstr[4];
	cstr[0] = 'a';
	cstr[1] = 'b';
	cstr[2] = 'c';
	cstr[3] = '\0';

	mara_value_t abc = mara_new_symbol(ctx, mara_str_from_cstr(cstr));
	mara_value_t abc2 = mara_new_symbol(ctx, mara_str_from_literal("abc"));
	ASSERT_EQ(abc, abc2);

	cstr[2] = 'f';
	mara_value_t abf = mara_new_symbol(ctx, mara_str_from_cstr(cstr));
	ASSERT_NE(abf, abc);

	mara_value_t abf2 = mara_new_symbol(ctx, mara_str_from_literal("abf"));
	ASSERT_EQ(abf, abf2);
}

TEST(runtime, symbol) {
	mara_exec(fixture.env, (mara_callback_t){ .fn = test_symbol });
}
