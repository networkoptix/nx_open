// Copyright 2017 Network Optix, Inc. Licensed under GNU Lesser General Public License version 3.
#pragma once

/**@file
 * Rudimentary standalone unit testing framework designed to mimic Google Test to a certain degree.
 */

#include <functional>
#include <iostream>
#include <sstream>
#include <string>

#if !defined(NX_KIT_API)
    #define NX_KIT_API
#endif

namespace nx {
namespace kit {
namespace test {

#define TEST(TEST_CASE, TEST_NAME) \
    static void test_##TEST_CASE##_##TEST_NAME(); \
    static const int unused_##TEST_CASE##_##TEST_NAME = \
        nx::kit::test::detail::regTest( \
            {#TEST_CASE, #TEST_NAME, &test_##TEST_CASE##_##TEST_NAME}); \
    static void test_##TEST_CASE##_##TEST_NAME()

#define ASSERT_TRUE(COND) \
    nx::kit::test::detail::assertBool(true, (COND), #COND, __FILE__, __LINE__)

#define ASSERT_FALSE(COND) \
    nx::kit::test::detail::assertBool(false, (COND), #COND, __FILE__, __LINE__)

#define ASSERT_EQ(EXPECTED, ACTUAL) \
    nx::kit::test::detail::assertEq( \
        (EXPECTED), #EXPECTED, (ACTUAL), #ACTUAL, __FILE__, __LINE__)

#define ASSERT_STREQ(EXPECTED, ACTUAL) \
    nx::kit::test::detail::assertEq( \
        std::string(EXPECTED), #EXPECTED, \
        std::string(ACTUAL), #ACTUAL, __FILE__, __LINE__)

/**
 * Usage: call from main():
 * <pre><code>
 *     return nx::kit::test::runAllTests();
 * </code></pre>
 * @return Number of failed tests.
 */
NX_KIT_API int runAllTests();

//-------------------------------------------------------------------------------------------------
// Temp files

/**
 * @return Path for the dir to create temp files, including the trailing path separator.
 */
NX_KIT_API std::string tempDir();

/**
 * Generates a random unique file name to avoid collisions in the tests. The file will be deleted
 * in the destructor, unless an exception is thrown (e.g. a test has failed), or
 * NX_KIT_TEST_KEEP_TEMP_FILES macro is defined (useful for debugging).
 *
 * ATTENTION: No safety is guaranteed - the uniqueness is based only on rand(), randomized with the
 * current time. A safer but more complex implementation could be based on std::tmpfile().
 */
class NX_KIT_API TempFile
{
public:
    TempFile(const std::string& prefix, const std::string& suffix);
    ~TempFile();

    std::string filename() const { return m_filename; }

    /*private*/ struct KeepFilesInitializer { KeepFilesInitializer() { keepFiles() = true; } };

private:
    static bool& keepFiles(); /**< @return Reference to its internal static bool variable. */

private:
    std::string m_filename;
};

//-------------------------------------------------------------------------------------------------
// Implementation

#if defined(NX_KIT_TEST_KEEP_TEMP_FILES)
    namespace {
        static const nx::kit::test::TempFile::KeepFilesInitializer tempFileKeepFilesInitializer;
    } // namespace
#endif

namespace detail {

NX_KIT_API void failEq(
    const std::string& expectedValue, const char* expectedExpr,
    const std::string& actualValue, const char* actualExpr,
    const char* const file, int line);

typedef std::function<void()> TestFunc;

struct Test
{
    std::string testCase;
    std::string testName;
    TestFunc testFunc;
};

NX_KIT_API int regTest(const Test& test);

NX_KIT_API void assertBool(
    bool expected, bool cond, const char* const condStr, const char* const file, int line);

template<typename Expected, typename Actual>
void assertEq(
    const Expected& expected, const char* expectedExpr,
    const Actual& actual, const char* actualExpr,
    const char* const file, int line)
{
    if (!(expected == actual)) //< Require only operator==().
    {
        std::ostringstream expectedValue;
        expectedValue << expected;
        std::ostringstream actualValue;
        actualValue << actual;
        detail::failEq(
            expectedValue.str(), expectedExpr, actualValue.str(), actualExpr, file, line);
    }
}

} // namespace detail

} // namespace test
} // namespace kit
} // namespace nx
