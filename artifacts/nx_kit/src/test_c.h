#pragma once

/**@file
 * Rudimentary header-only unit testing framework for C99. Supports a single test only.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

static void test_detail_runTest(void);

int main()
{
    test_detail_runTest();
    fprintf(stderr, "\nSUCCESS: Test PASSED.\n");
    return 0;
}

// Use instead of "main()".
#define TEST() \
    static void test_detail_runTest(void) /* Function body follows the TEST macro. */

#define ASSERT_TRUE(COND) test_detail_assertBool(true, (COND), #COND, __LINE__)
#define ASSERT_FALSE(COND) test_detail_assertBool(false, (COND), #COND, __LINE__)

#define ASSERT_STREQ(EXPECTED, ACTUAL) \
    test_detail_assertStrEq((EXPECTED), #EXPECTED, (ACTUAL), #ACTUAL, __LINE__)

#define ASSERT_EQ(EXPECTED, ACTUAL) \
    test_detail_assertIntEq((EXPECTED), #EXPECTED, (ACTUAL), #ACTUAL, __LINE__)

static void test_detail_assertStrEq(
    const char* expected, const char* expectedStr,
    const char* actual, const char* actualStr,
    int line)
{
    if (strcmp(expected, actual) != 0)
    {
        fprintf(stderr, "\nTest FAILED at line %d:\n", line);
        fprintf(stderr, "    Expected: [%s] (%s)\n", expected, expectedStr);
        fprintf(stderr, "    Actual: [%s] (%s)\n", actual, actualStr);
        exit(1);
    }
}

static void test_detail_assertIntEq(
    int expected, const char* expectedStr,
    int actual, const char* actualStr,
    int line)
{
    if (expected != actual)
    {
        fprintf(stderr, "\nTest FAILED at line %d:\n", line);
        fprintf(stderr, "    Expected: [%d] (%s)\n", expected, expectedStr);
        fprintf(stderr, "    Actual: [%d] (%s)\n", actual, actualStr);
        exit(1);
    }
}

static void test_detail_assertBool(bool expected, bool cond, const char* condStr, int line)
{
    if (expected != cond)
    {
        fprintf(stderr, "\nTest FAILED at line %d:\n", line);
        fprintf(stderr, "    %s: %s", (expected == true) ? "False" : "True", condStr);
        exit(1);
    }
}
