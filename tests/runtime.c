#include "rktest.h"
#include <mara.h>
#include <mara/utils.h>
#include <mara/bind.h>
#include "common.h"

static mara_fixture_t fixture;

typedef struct {
	int num_elements;
} iterator_state_t;

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

	mara_value_t abc = mara_new_sym(fixture.ctx, mara_str_from_cstr(cstr));
	mara_value_t abc2 = mara_new_sym(fixture.ctx, mara_str_from_literal("abc"));
	ASSERT_EQ(abc.internal, abc2.internal);

	cstr[2] = 'f';
	mara_value_t abf = mara_new_sym(fixture.ctx, mara_str_from_cstr(cstr));
	ASSERT_NE(abf.internal, abc.internal);

	mara_value_t abf2 = mara_new_sym(fixture.ctx, mara_str_from_literal("abf"));
	ASSERT_EQ(abf.internal, abf2.internal);
}

static inline mara_error_t*
iterate_map(mara_exec_ctx_t* ctx, int argc, const mara_value_t* argv, void* userdata, mara_value_t* result) {
	(void)ctx;
	(void)argc;
	(void)argv;

	iterator_state_t* itr_state = userdata;

	if (++itr_state->num_elements > 2) {
		*result = mara_value_from_bool(false);
	}

	return NULL;
}

TEST(runtime, map) {
	mara_exec_ctx_t* ctx = fixture.ctx;
	mara_map_t* map = mara_new_map(ctx, mara_get_local_zone(ctx));

	mara_index_t len = mara_map_len(ctx, map);
	ASSERT_EQ(len, 0);

	mara_value_t a = mara_new_sym(ctx, mara_str_from_literal("a"));
	mara_value_t b = mara_new_sym(ctx, mara_str_from_literal("b"));
	ASSERT_NE(a.internal, b.internal);

	mara_value_t value = mara_map_get(ctx, map, a);
	ASSERT_TRUE(mara_value_is_nil(value));

	mara_map_set(ctx, map, a, mara_value_from_int(1));
	mara_map_set(ctx, map, b, mara_value_from_int(2));
	len = mara_map_len(ctx, map);
	ASSERT_EQ(len, 2);

	value = mara_map_get(ctx, map, a);
	ASSERT_TRUE(mara_value_is_int(value));
	mara_index_t value_int;
	MARA_ASSERT_NO_ERROR(ctx, mara_value_to_int(ctx, value, &value_int));
	ASSERT_EQ(value_int, 1);

	value = mara_map_get(ctx, map, b);
	ASSERT_TRUE(mara_value_is_int(value));
	MARA_ASSERT_NO_ERROR(ctx, mara_value_to_int(ctx, value, &value_int));
	ASSERT_EQ(value_int, 2);

	iterator_state_t iterator_state = { 0 };
	mara_fn_t* fn = mara_new_fn(ctx, mara_get_local_zone(ctx), iterate_map, &iterator_state);
	mara_map_foreach(ctx, map, fn);

	ASSERT_EQ(iterator_state.num_elements, 2);
}
