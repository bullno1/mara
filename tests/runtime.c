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

TEST(runtime, map) {
	mara_exec_ctx_t* ctx = fixture.ctx;
	mara_value_t map = mara_new_map(ctx, mara_get_local_zone(ctx));

	mara_index_t len;
	MARA_ASSERT_NO_ERROR(ctx, mara_map_len(ctx, map, &len));
	ASSERT_EQ(len, 0);

	mara_value_t a = mara_new_symbol(ctx, mara_str_from_literal("a"));
	mara_value_t b = mara_new_symbol(ctx, mara_str_from_literal("b"));
	ASSERT_NE(a, b);

	mara_value_t value;
	MARA_ASSERT_NO_ERROR(ctx, mara_map_get(ctx, map, a, &value));
	ASSERT_TRUE(mara_value_is_nil(value));

	MARA_ASSERT_NO_ERROR(ctx, mara_map_set(ctx, map, a, mara_value_from_int(1)));
	MARA_ASSERT_NO_ERROR(ctx, mara_map_set(ctx, map, b, mara_value_from_int(2)));
	MARA_ASSERT_NO_ERROR(ctx, mara_map_len(ctx, map, &len));
	ASSERT_EQ(len, 2);

	MARA_ASSERT_NO_ERROR(ctx, mara_map_get(ctx, map, a, &value));
	ASSERT_TRUE(mara_value_is_int(value));
	mara_index_t value_int;
	MARA_ASSERT_NO_ERROR(ctx, mara_value_to_int(ctx, value, &value_int));
	ASSERT_EQ(value_int, 1);

	MARA_ASSERT_NO_ERROR(ctx, mara_map_get(ctx, map, b, &value));
	ASSERT_TRUE(mara_value_is_int(value));
	MARA_ASSERT_NO_ERROR(ctx, mara_value_to_int(ctx, value, &value_int));
	ASSERT_EQ(value_int, 2);
}
