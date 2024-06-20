#include "rktest.h"
#include <mara.h>
#include <mara/utils.h>
#include "common.h"

static mara_fixture_t fixture;

TEST_SETUP(runtime) {
	setup_mara_fixture(&fixture);
}

TEST_TEARDOWN(runtime) {
	teardown_mara_fixture(&fixture);
}

TEST(runtime, symbol) {
	char cstr[4];
	cstr[0] = 'a';
	cstr[1] = 'b';
	cstr[2] = 'c';
	cstr[3] = '\0';

	mara_value_t abc = mara_new_symbol(fixture.ctx, mara_str_from_cstr(cstr));
	mara_value_t abc2 = mara_new_symbol(fixture.ctx, mara_str_from_literal("abc"));
	ASSERT_EQ(abc, abc2);

	cstr[2] = 'f';
	mara_value_t abf = mara_new_symbol(fixture.ctx, mara_str_from_cstr(cstr));
	ASSERT_NE(abf, abc);

	mara_value_t abf2 = mara_new_symbol(fixture.ctx, mara_str_from_literal("abf"));
	ASSERT_EQ(abf, abf2);
}
