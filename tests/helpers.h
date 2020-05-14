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

#define mara_assert_enum(ENUM_TYPE, EXPECTED, OP, ACTUAL) \
	do { \
		unsigned int expected = (EXPECTED); \
		unsigned int actual = (ACTUAL); \
		if(!(expected OP actual)) { \
			munit_errorf( \
				"assert failed: " #EXPECTED " " #OP " " #ACTUAL " (%s " #OP " %s)", \
				ENUM_TYPE##_to_str(expected), ENUM_TYPE##_to_str(actual) \
			); \
		} \
	} while (0)

#define mara_assert_source_coord_equal(EXPECTED, ACTUAL) \
	do { \
		mara_source_coord_t loc_expected = (EXPECTED); \
		mara_source_coord_t loc_actual = (ACTUAL); \
		munit_assert_uint(loc_expected.column, ==, loc_actual.column); \
		munit_assert_uint(loc_expected.line, ==, loc_actual.line); \
	} while (0)

#define mara_assert_source_range_equal(EXPECTED, ACTUAL) \
	do { \
		mara_source_range_t loc_range_expected = (EXPECTED); \
		mara_source_range_t loc_range_actual = (ACTUAL); \
		mara_assert_source_coord_equal(loc_range_expected.start, loc_range_actual.start); \
		mara_assert_source_coord_equal(loc_range_expected.end, loc_range_actual.end); \
	} while(0)

mara_context_config_t
mara_default_context_config();

#endif
