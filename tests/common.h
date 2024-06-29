#ifndef MARA_TESTS_COMMON_H
#define MARA_TESTS_COMMON_H

#include <mara.h>
#include <mara/utils.h>
#include "rktest.h"

#define MARA_ASSERT_NO_ERROR(exec_ctx, ...) \
	do { \
		mara_error_t* error = __VA_ARGS__; \
		if (error != NULL) { \
			if (rktest_filenames_enabled()) { \
				printf("%s(%d): ", __FILE__, __LINE__); \
			} \
			printf("error: Mara call returned with error\n"); \
			printf("%s\n", #__VA_ARGS__); \
			mara_print_error(exec_ctx, error, (mara_print_options_t){ 0 }, (mara_writer_t){ \
				.fn = mara_write_to_file, \
				.userdata = stdout, \
			}); \
			rktest_fail_current_test();                                                                  \
		} \
	} while (0)

#define MARA_ASSERT_STR_EQ(lhs, rhs) MARA_CHECK_STR_EQ((lhs), (rhs), RKTEST_CHECK_ASSERT, " ")
#define MARA_CHECK_STR_EQ(lhs, rhs, is_assert, ...) \
	do {                                                                                                 \
		mara_str_t lhs_value = lhs;                                                                      \
		mara_str_t rhs_value = rhs;                                                                      \
		if (!mara_str_equal(lhs_value, rhs_value)) {                                                     \
			if (rktest_filenames_enabled()) {                                                            \
				printf("%s(%d): ", __FILE__, __LINE__);                                                  \
			}                                                                                            \
			printf("error: Expected equality of these values:\n");                                       \
			printf("  %s\n", #lhs);                                                                      \
			const bool lhs_is_literal = (#lhs)[0] == '"';                                                \
			if (!lhs_is_literal)                                                                         \
				printf("    Which is: {%.*s}\n", lhs_value.len, lhs_value.data);                           \
			printf("  %s\n", #rhs);                                                                      \
			const bool rhs_is_literal = (#rhs)[0] == '"';                                                \
			if (!rhs_is_literal)                                                                         \
				printf("    Which is: {%.*s}\n", rhs_value.len, rhs_value.data);                           \
			printf(__VA_ARGS__);                                                                         \
			printf("\n");                                                                                \
			rktest_fail_current_test();                                                                  \
		}                                                                                                \
	} while (0)

#define MARA_INLINE_SOURCE \
	mara_str_from_literal((__FILE__ "(" MARA_LINE_STRING ")"))
#define MARA_LINE_STRING MARA_TEST_STRINGIFY(__LINE__)
#define MARA_TEST_STRINGIFY(x) MARA_TEST_STRINGIFY2(x)
#define MARA_TEST_STRINGIFY2(x) #x

typedef struct {
	mara_env_t* env;
	mara_exec_ctx_t* ctx;
} mara_fixture_t;

static inline void
setup_mara_fixture(mara_fixture_t* fixture) {
	fixture->env = mara_create_env((mara_env_options_t){ 0 });
	fixture->ctx = mara_begin(fixture->env, (mara_exec_options_t){ 0 });
}

static inline void
teardown_mara_fixture(mara_fixture_t* fixture) {
	mara_end(fixture->ctx);
	mara_destroy_env(fixture->env);
}

#endif
