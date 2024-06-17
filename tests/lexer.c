#include "rktest.h"
#include <mara.h>
#include <mara/utils.h>

struct {
	mara_env_t* env;
} fixture;

TEST_SETUP(lexer) {
	fixture.env = mara_create_env((mara_env_options_t){ 0 });
}

TEST_TEARDOWN(lexer) {
	mara_destroy_env(fixture.env);
}

static inline void
test_basic(mara_exec_ctx_t* ctx, void* userdata) {
	(void)ctx;
	(void)userdata;
}

TEST(lexer, basic) {
	mara_exec(fixture.env, (mara_callback_t){ .fn = test_basic });
}
