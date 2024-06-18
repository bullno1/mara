#ifndef MARA_TESTS_COMMON_H
#define MARA_TESTS_COMMON_H

#include <mara.h>

typedef struct mara_test_thunk_data_s {
	void (*fn)(mara_exec_ctx_t* ctx);
} mara_test_thunk_data_t;

#define MARA_ASSERT_NO_ERROR(CALL) \
	do { \
		mara_error_t* error = CALL; \
		ASSERT_TRUE_INFO( \
			error == NULL, \
			"%.*s: %.*s", \
			error->type.len, error->type.data, \
			error->message.len, error->message.data \
		); \
	} while (0)

#define MARA_TEST(SUITE_NAME, TEST_NAME) \
	static void test##_##SUITE_NAME##_##TEST_NAME(mara_exec_ctx_t* ctx); \
	TEST(SUITE_NAME, TEST_NAME) { \
		mara_exec(fixture.env, (mara_callback_t){ \
			.fn = mara_test_thunk, \
			.userdata = &(mara_test_thunk_data_t){ \
				.fn = test##_##SUITE_NAME##_##TEST_NAME \
			} \
		}); \
	} \
	void test##_##SUITE_NAME##_##TEST_NAME(mara_exec_ctx_t* ctx)

static inline void
mara_test_thunk(mara_exec_ctx_t* ctx, void* userdata) {
	mara_test_thunk_data_t* thunk_data = userdata;
	thunk_data->fn(ctx);
}

#endif
