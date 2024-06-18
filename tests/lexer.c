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
	(void)userdata;

	mara_str_t str = mara_str_from_literal(
		"test 1.2 ( ) { }\n"
		" \"Hello\"\r\n"
		"    \twhat? \r"
		"69"
	);
	mara_str_reader_t str_reader;
	mara_value_t result;
	mara_error_t* error = mara_parse_all(
		ctx,
		mara_get_local_zone(ctx),
		mara_str_from_literal(__FILE__),
		mara_init_str_reader(&str_reader, str),
		&result
	);
	ASSERT_TRUE_INFO(
		error == NULL,
		"%.*s", error->type.len, error->type.data
	);
}

TEST(lexer, basic) {
	mara_exec(fixture.env, (mara_callback_t){ .fn = test_basic });
}
