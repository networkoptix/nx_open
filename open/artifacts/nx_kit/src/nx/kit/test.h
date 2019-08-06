// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

/**@file
 * Rudimentary standalone unit testing framework designed to mimic Google Test to a certain degree.
 */

#include <functional>
#include <iostream>
#include <sstream>
#include <string>

#include <nx/kit/utils.h>

#if !defined(NX_KIT_API)
    #define NX_KIT_API
#endif

namespace nx {
namespace kit {
namespace test {

extern bool NX_KIT_API verbose; //< Use to control additional output of the unit test framework.

 /**
  * Main macro for defining a test function. Calls a helper macro - such trick allows to comment
  * out all tests except one by redefining TEST to DISABLED_TEST, and changing that one test to use
  * ENABLED_TEST instead of TEST.
  *
  * Usage:
  * ```
  *     TEST(MySuite, myTest)
  *     {
  *         int var = 42;
  *         ASSERT_EQ(42, var);
  *     }
  * ```
  */
#define TEST(TEST_CASE, TEST_NAME) ENABLED_TEST(TEST_CASE, TEST_NAME)

#define ENABLED_TEST(TEST_CASE, TEST_NAME) \
    static void test_##TEST_CASE##_##TEST_NAME(); \
    int unused_##TEST_CASE##_##TEST_NAME /* Not `static const` to suppress "unused" warning. */ = \
        ::nx::kit::test::detail::regTest( \
            {#TEST_CASE, #TEST_NAME, #TEST_CASE "." #TEST_NAME, test_##TEST_CASE##_##TEST_NAME, \
                /*tempDir*/ ""}); \
    static void test_##TEST_CASE##_##TEST_NAME()
    // Function body follows the DEFINE_TEST macro.

#define DISABLED_TEST(TEST_CASE, TEST_NAME) \
    static void disabled_test_##TEST_CASE##_##TEST_NAME() /* The function will be unused. */
    // Function body follows the DISABLED_TEST macro.

#define ASSERT_TRUE(CONDITION) \
    ::nx::kit::test::detail::assertBool(true, !!(CONDITION), #CONDITION, __FILE__, __LINE__)

#define ASSERT_TRUE_AT_LINE(LINE, CONDITION) \
    ::nx::kit::test::detail::assertBool(true, !!(CONDITION), #CONDITION, __FILE__, LINE, __LINE__)

#define ASSERT_FALSE(CONDITION) \
    ::nx::kit::test::detail::assertBool(false, !!(CONDITION), #CONDITION, __FILE__, __LINE__)

#define ASSERT_FALSE_AT_LINE(LINE, CONDITION) \
    ::nx::kit::test::detail::assertBool(false, !!(CONDITION), #CONDITION, __FILE__, LINE, __LINE__)

#define ASSERT_EQ(EXPECTED, ACTUAL) \
    ::nx::kit::test::detail::assertEq( \
        (EXPECTED), #EXPECTED, (ACTUAL), #ACTUAL, __FILE__, __LINE__)

#define ASSERT_EQ_AT_LINE(LINE, EXPECTED, ACTUAL) \
    ::nx::kit::test::detail::assertEq( \
        (EXPECTED), #EXPECTED, (ACTUAL), #ACTUAL, __FILE__, LINE, __LINE__)

#define ASSERT_STREQ(EXPECTED, ACTUAL) \
    ::nx::kit::test::detail::assertStrEq( \
        ::nx::kit::test::detail::toCStr(EXPECTED), #EXPECTED, \
        ::nx::kit::test::detail::toCStr(ACTUAL), #ACTUAL, __FILE__, __LINE__)

#define ASSERT_STREQ_AT_LINE(LINE, EXPECTED, ACTUAL) \
    ::nx::kit::test::detail::assertStrEq( \
        ::nx::kit::test::detail::toCStr(EXPECTED), #EXPECTED, \
        ::nx::kit::test::detail::toCStr(ACTUAL), #ACTUAL, __FILE__, LINE, __LINE__)

/**
 * Should be called for regular tests, from the TEST() body.
 * @return Path to the directory to create temp files in, including the trailing path separator:
 *     "base-temp-dir/case.test/", where "base-temp-dir" can be assigned with "--tmp" command line
 *     option and by default is "system-temp-dir/nx_kit_test_#", where # is a random number. The
 *     directory is created (if already exists - a fatal error is produced).
 */
NX_KIT_API const char* tempDir();

/**
 * Should be called for tests that require temp dir outside the TEST() body, e.g. from a static
 * initialization code.
 * @return Path to the directory to create temp files in, including the trailing path separator:
 *     "base-temp-dir/static/", where "base-temp-dir" is the same as for tempDir(). The directory
 *     is created (if already exists - a fatal error is produced).
 */
NX_KIT_API const char* staticTempDir();

/**
 * Usage: call from main():
 * <pre><code>
 *
 *     int main()
 *     {
 *         return nx::kit::test::runAllTests("myTests");
 *     }
 *
 * </code></pre>
 * @return Number of failed tests.
 */
NX_KIT_API int runAllTests(const char *testSuiteName);

NX_KIT_API void createFile(const char* filename, const char* content);

//-------------------------------------------------------------------------------------------------
// Implementation

#if defined(NX_KIT_TEST_KEEP_TEMP_FILES)
    namespace
    {
        static const nx::kit::test::TempFile::KeepFilesInitializer tempFileKeepFilesInitializer;
    }
#endif

namespace detail {

NX_KIT_API void failEq(
    const char* expectedValue, const char* expectedExpr,
    const char* actualValue, const char* actualExpr,
    const char* file, int line, int actualLine = -1);

typedef std::function<void()> TestFunc;

struct Test
{
    const char* const testCase;
    const char* const testName;
    const char* const testCaseDotName;
    const TestFunc testFunc;
    std::string tempDir;
};

NX_KIT_API int regTest(const Test& test);

NX_KIT_API void assertBool(
    bool expected, bool condition, const char* conditionStr,
    const char* file, int line, int actualLine = -1);

template<typename Expected, typename Actual>
void assertEq(
    const Expected& expected, const char* expectedExpr,
    const Actual& actual, const char* actualExpr,
    const char* file, int line, int actualLine = -1)
{
    if (!(expected == actual)) //< Require only operator==().
    {
        detail::failEq(
            utils::toString(expected).c_str(), expectedExpr,
            utils::toString(actual).c_str(), actualExpr,
            file, line, actualLine);
    }
}

NX_KIT_API void assertStrEq(
    const char* expectedValue, const char* expectedExpr,
    const char* actualValue, const char* actualExpr,
    const char* file, int line, int actualLine = -1);

inline const char* toCStr(const std::string& s)
{
    return s.c_str();
}

inline const char* toCStr(const char* s)
{
    return s;
}

} // namespace detail

} // namespace test
} // namespace kit
} // namespace nx
