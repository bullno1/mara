#include <stdio.h>
#include <stdlib.h>
#include <munit/munit.h>
#define BK_IMPLEMENTATION
#include <bk/default_allocator.h>

#define TEST_SUITE(X) \
    X(context) \
    X(strpool) \

#define DECLARE_SUITE(S) extern MunitSuite S;
#define IMPORT_SUITE(S) S,

TEST_SUITE(DECLARE_SUITE)

int
main(int argc, char* argv[])
{
    MunitSuite all_suites[] = {
        TEST_SUITE(IMPORT_SUITE)
        { 0 }
    };

    MunitSuite main_suite = {
        .prefix = "",
        .suites = all_suites,
    };

    return munit_suite_main(&main_suite, NULL, argc, argv);
}
