#pragma once

/**@file
 * Rudimentary header-only unit testing framework for C99. Each test should be called manually.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/**
 * Defines a test function: int TEST_FUNC(), returning 0 if test passes and 1 if test fails.
 */
#define TEST(TEST_FUNC) \
    static void nx_test_detail_test_##TEST_FUNC(void); \
    int TEST_FUNC() \
    { \
        fprintf(stderr, "\nRunning C99 test: %s\n", #TEST_FUNC); \
        fprintf(stderr, "%s\n\n", nx_test_detail_separator); \
        nx_test_detail_test_##TEST_FUNC(); \
        return nx_test_detail_finish_test(); \
    } \
    static void nx_test_detail_test_##TEST_FUNC(void) /* Function body follows the TEST macro. */

#define ASSERT_TRUE(COND) do \
{ \
    if (!nx_test_detail_assertBool(true, (COND), #COND, __LINE__)) \
        return; \
} while (0)

#define ASSERT_FALSE(COND) do \
{ \
    if (!nx_test_detail_assertBool(false, (COND), #COND, __LINE__)) \
        return; \
} while (0)

#define ASSERT_STREQ(EXPECTED, ACTUAL) do \
{ \
    if (!nx_test_detail_assertStrEq((EXPECTED), #EXPECTED, (ACTUAL), #ACTUAL, __LINE__)) \
        return; \
} while (0)

#define ASSERT_EQ(EXPECTED, ACTUAL) do \
{ \
    if (!nx_test_detail_assertIntEq((EXPECTED), #EXPECTED, (ACTUAL), #ACTUAL, __LINE__)) \
        return; \
} while (0)

//-------------------------------------------------------------------------------------------------
// Implementation

static const char* const nx_test_detail_separator =
    "==================================================================";

static bool nx_test_detail_failed = false;

static int nx_test_detail_finish_test(void)
{
    fprintf(stderr, "\n%s\n", nx_test_detail_separator);
    if (nx_test_detail_failed)
    {
        fprintf(stderr, "Test FAILED. See the messages above.\n");
        return 1;
    }
    fprintf(stderr, "SUCCESS: Test PASSED.\n");
    return 0;
}

static bool nx_test_detail_assertStrEq(
    const char* expected, const char* expectedStr,
    const char* actual, const char* actualStr,
    int line)
{
    if (strcmp(expected, actual) != 0)
    {
        fprintf(stderr, "\nTest FAILED at line %d:\n", line);
        fprintf(stderr, "    Expected: [%s] (%s)\n", expected, expectedStr);
        fprintf(stderr, "    Actual: [%s] (%s)\n", actual, actualStr);
        nx_test_detail_failed = true;
        return false;
    }
    return true;
}

static bool nx_test_detail_assertIntEq(
    int expected, const char* expectedStr,
    int actual, const char* actualStr,
    int line)
{
    if (expected != actual)
    {
        fprintf(stderr, "\nTest FAILED at line %d:\n", line);
        fprintf(stderr, "    Expected: [%d] (%s)\n", expected, expectedStr);
        fprintf(stderr, "    Actual: [%d] (%s)\n", actual, actualStr);
        nx_test_detail_failed = true;
        return false;
    }
    return true;
}

static bool nx_test_detail_assertBool(bool expected, bool cond, const char* condStr, int line)
{
    if (expected != cond)
    {
        fprintf(stderr, "\nTest FAILED at line %d:\n", line);
        fprintf(stderr, "    %s: %s\n", (expected == true) ? "False" : "True", condStr);
        nx_test_detail_failed = true;
        return false;
    }
    return true;
}
