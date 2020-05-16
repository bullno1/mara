#include <munit/munit.h>
#include "../src/internal.h"
#include "../src/strpool.h"
#include <math.h>


static MunitResult
nanbox_assignment(const MunitParameter params[], void* fixture)
{
	mara_value_t value, value2;

	mara_value_set_null(&value);
	munit_assert_true(mara_value_type_check(&value, MARA_VAL_NULL));
	munit_assert_false(mara_value_type_check(&value, MARA_VAL_BOOL));
	munit_assert_false(mara_value_type_check(&value, MARA_VAL_NUMBER));
	munit_assert_false(mara_value_type_check(&value, MARA_VAL_GC_OBJ));

	value2 = value;
	munit_assert_true(mara_value_type_check(&value2, MARA_VAL_NULL));
	munit_assert_false(mara_value_type_check(&value2, MARA_VAL_BOOL));
	munit_assert_false(mara_value_type_check(&value2, MARA_VAL_NUMBER));
	munit_assert_false(mara_value_type_check(&value2, MARA_VAL_GC_OBJ));

	mara_value_set_bool(&value, true);
	munit_assert_false(mara_value_type_check(&value, MARA_VAL_NULL));
	munit_assert_true(mara_value_type_check(&value, MARA_VAL_BOOL));
	munit_assert_false(mara_value_type_check(&value, MARA_VAL_NUMBER));
	munit_assert_false(mara_value_type_check(&value, MARA_VAL_GC_OBJ));
	munit_assert_true(mara_value_as_bool(&value));

	mara_value_set_bool(&value, false);
	munit_assert_false(mara_value_type_check(&value, MARA_VAL_NULL));
	munit_assert_true(mara_value_type_check(&value, MARA_VAL_BOOL));
	munit_assert_false(mara_value_type_check(&value, MARA_VAL_NUMBER));
	munit_assert_false(mara_value_type_check(&value, MARA_VAL_GC_OBJ));
	munit_assert_false(mara_value_as_bool(&value));

	value2 = value;
	munit_assert_false(mara_value_type_check(&value2, MARA_VAL_NULL));
	munit_assert_true(mara_value_type_check(&value2, MARA_VAL_BOOL));
	munit_assert_false(mara_value_type_check(&value2, MARA_VAL_NUMBER));
	munit_assert_false(mara_value_type_check(&value2, MARA_VAL_GC_OBJ));
	munit_assert_false(mara_value_as_bool(&value2));

	mara_gc_header_t header;
	mara_value_set_gc_obj(&value, &header);
	munit_assert_false(mara_value_type_check(&value, MARA_VAL_NULL));
	munit_assert_false(mara_value_type_check(&value, MARA_VAL_BOOL));
	munit_assert_false(mara_value_type_check(&value, MARA_VAL_NUMBER));
	munit_assert_true(mara_value_type_check(&value, MARA_VAL_GC_OBJ));
	munit_assert_ptr_equal(&header, mara_value_as_gc_obj(&value));

	mara_value_set_gc_obj(&value, NULL);
	munit_assert_false(mara_value_type_check(&value, MARA_VAL_NULL));
	munit_assert_false(mara_value_type_check(&value, MARA_VAL_BOOL));
	munit_assert_false(mara_value_type_check(&value, MARA_VAL_NUMBER));
	munit_assert_true(mara_value_type_check(&value, MARA_VAL_GC_OBJ));
	munit_assert_ptr_equal(NULL, mara_value_as_gc_obj(&value));

    return MUNIT_OK;
}

static void
test_numeric_assignment(double number)
{
	mara_value_t value;
	mara_value_set_number(&value, number);
	munit_assert_false(mara_value_type_check(&value, MARA_VAL_NULL));
	munit_assert_false(mara_value_type_check(&value, MARA_VAL_BOOL));
	munit_assert_true(mara_value_type_check(&value, MARA_VAL_NUMBER));
	munit_assert_false(mara_value_type_check(&value, MARA_VAL_GC_OBJ));

	if(!isnan(number))
	{
		munit_assert_double(number, ==, mara_value_as_number(&value));
	}
	else
	{
		munit_assert_memory_equal(sizeof(double), &number, &value);
	}
}

static MunitResult
numeric_assignment(const MunitParameter params[], void* fixture)
{
	test_numeric_assignment(0.0);
	test_numeric_assignment(M_PI);
	test_numeric_assignment(NAN);
	test_numeric_assignment(NAN + 2);
	test_numeric_assignment(INFINITY);
	test_numeric_assignment(-INFINITY);
	test_numeric_assignment(INFINITY * NAN);
	test_numeric_assignment(INFINITY / INFINITY);
	test_numeric_assignment(0.1 + 0.2);

    return MUNIT_OK;
}

static MunitTest tests[] = {
    {
        .name = "/nanbox_assginment",
        .test = nanbox_assignment,
    },
    {
        .name = "/numeric_assginment",
        .test = numeric_assignment,
    },
    { .test = NULL }
};

MunitSuite value = {
    .prefix = "/value",
    .tests = tests
};
