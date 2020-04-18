#ifndef MARA_TEST_HELPERS
#define MARA_TEST_HELPERS

#include <munit/munit.h>
#include <mara.h>

#define mara_assert_string_ref_equal(EXPECTED, ACTUAL) \
	do { \
		mara_string_ref_t expected = (EXPECTED); \
		mara_string_ref_t actual = (ACTUAL); \
		if(!mara_string_ref_equal(expected, actual)) { \
			munit_errorf( \
				"assert failed: " #EXPECTED " == " #ACTUAL " (\"%.*s\" == \"%.*s\")", \
				(int)expected.length, expected.ptr, \
				(int)actual.length, actual.ptr \
			); \
		} \
	} while (0)

#endif
