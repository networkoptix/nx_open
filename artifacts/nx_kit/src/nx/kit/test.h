// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

/**@file
 * Rudimentary standalone unit testing framework designed to mimic Google Test to a certain degree.
 */

#include <functional>
#include <iostream>
#include <sstream>
#include <stdint.h>
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
        EXPECTED, #EXPECTED, ACTUAL, #ACTUAL, __FILE__, __LINE__)

#define ASSERT_STREQ_AT_LINE(LINE, EXPECTED, ACTUAL) \
    ::nx::kit::test::detail::assertStrEq( \
        EXPECTED, #EXPECTED, ACTUAL, #ACTUAL, __FILE__, LINE, __LINE__)

/**
 * Tests that the given string has the expected value, having replaced all occurrences of the given
 * substring with the given replacement (placeholder) - useful when the string being tested
 * contains some substring (e.g. a file name) known only at runtime. Zero bytes inside the strings
 * are supported.
 *
 * If the strings are not equal, they are treated as multiline via '\n' - line-by-line exact
 * comparison is performed, and the different lines are printed. Also for convenience the actual
 * string is printed as a multiline text with line numbers, non-printable chars as '?', and
 * trailing spaces as '#'.
 *
 * @param actualSubstrToReplace Must be empty if the replacement is not needed.
 * @param actualSubstrReplacement Must be empty if the replacement is not needed.
 */
NX_KIT_API void assertMultilineTextEquals(
    const char* file, int line, const std::string& testCaseTag,
    const std::string& expected, const std::string& actual,
    const std::string actualSubstrToReplace = "", const std::string& actualSubstrReplacement = "");

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
NX_KIT_API int runAllTests(const char *testSuiteName, const char* specificArgsHelp = nullptr);

/** Allows zero bytes in the content. */
NX_KIT_API void createFile(const std::string& filename, const std::string& content);

//-------------------------------------------------------------------------------------------------
// Implementation

namespace detail {

#if defined(NX_KIT_TEST_KEEP_TEMP_FILES)
    static const nx::kit::test::TempFile::KeepFilesInitializer tempFileKeepFilesInitializer;
#endif

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

NX_KIT_API void failEq(
    const std::string& expectedValue, const char* expectedExpr,
    const std::string& actualValue, const char* actualExpr,
    const char* file, int line, int actualLine = -1);

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
            nx::kit::utils::toString(expected), expectedExpr,
            nx::kit::utils::toString(actual), actualExpr,
            file, line, actualLine);
    }
}

/**
 * Immutable string with distinct null and empty states, and capable of containing '\0' inside.
 * Fully supported by nx::kit::utils::toString() and nx/kit/test.h ASSERT_STREQ().
 */
struct UniversalString
{
    const std::string s;
    const bool isNull;

    UniversalString(const UniversalString& u, std::false_type): s(u.s), isNull(u.isNull) {}

    /** Creates from a string literal with possible '\0' inside. */
    template<size_t len>
    UniversalString(const char (&a)[len], std::true_type): s(a, len - 1), isNull(false) {}

    /** Creates from a string literal with possible '\0' inside. */
    template<size_t len>
    UniversalString(char (&a)[len], std::true_type): s(a, len - 1), isNull(false) {}

    UniversalString(const char* p, std::false_type): s(p ? p : ""), isNull(!p) {}
    UniversalString(std::string s, std::false_type): s(std::move(s)), isNull(false) {}

    /** Required for proper overloading resolution, to prefer array versions over char*. */
    template<typename T>
    UniversalString(T&& arg): UniversalString(
        std::forward<T>(arg), std::is_array<typename std::remove_reference<T>::type>())
    {
    }

    bool operator==(const UniversalString& u) const { return isNull == u.isNull && s == u.s; }
    bool operator!=(const UniversalString& u) const { return !(*this == u); }

    std::string toString() const
    {
        if (isNull)
            return "null";
        return nx::kit::utils::toString(s);
    }
};

inline std::ostream& operator<<(std::ostream& stream, const UniversalString& str)
{
    return stream << str.toString();
}

NX_KIT_API void assertStrEq(
    const UniversalString& expectedValue, const char* expectedExpr,
    const UniversalString& actualValue, const char* actualExpr,
    const char* file, int line, int actualLine = -1);

} // namespace detail

} // namespace test
} // namespace kit
} // namespace nx
