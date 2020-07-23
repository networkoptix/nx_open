// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

/**@file
 * Testing features of test.h.
 */

#include <nx/kit/test.h>

#include <fstream>
#include <string>

namespace nx {
namespace kit {
namespace test {
namespace test {

TEST(test, tempDir)
{
    const char* const tempDir = nx::kit::test::tempDir();
    ASSERT_TRUE(tempDir != nullptr);
    std::cerr << "Got tempDir: [" << tempDir << "]" << std::endl;
}

TEST(test, createFile)
{
    ASSERT_TRUE(nx::kit::test::tempDir() != nullptr);

    // Write some data to a test file, and read it back.

    static const char* const contents = "First line\nLine 2\n";

    const std::string filename = nx::kit::test::tempDir() + std::string("test.txt");

    nx::kit::test::createFile(filename, contents);

    std::ifstream inputStream(filename);
    ASSERT_TRUE(inputStream.is_open());
    std::string dataFromFile;
    std::string line;
    while (std::getline(inputStream, line))
        dataFromFile += line + "\n";
    inputStream.close();

    ASSERT_EQ(contents, dataFromFile);
}

template<typename Expected, typename Actual>
void assertStreqFails(int line, const Expected& expected, const Actual& actual)
{
    try
    {
        ASSERT_STREQ_AT_LINE(line, expected, actual);
    }
    catch (const std::exception& e)
    {
        ASSERT_STREQ(
            "    Expected: [" + detail::UniversalString(expected).toString() + "] (expected)\n"
            "    Actual:   [" + detail::UniversalString(actual).toString() + "] (actual)",
            e.what());
        return; //< The exception is expected.
    }
    ASSERT_TRUE(false); //< Fail the test if there was no exception.
}

TEST(test, assertStreqWithNulChars)
{
    const std::string strWithNulInside("a\0b", 3);

    // Check that ASSERT_STREQ works with all combinations of a char array and std::string.
    ASSERT_STREQ("a\0b", strWithNulInside);
    ASSERT_STREQ("a\0b", "a\0b");
    ASSERT_STREQ(strWithNulInside, strWithNulInside);
    ASSERT_STREQ(strWithNulInside, "a\0b");

    // If the checks above succeed, it may mean that '\0' in the middle is not supported at all,
    // thus, we add a second check that must fail - the strings differ in the part after '\0'.

    const std::string strWithNulInsideAndDifferentSuffix("a\0B", 3);

    assertStreqFails(__LINE__, "a\0b", strWithNulInsideAndDifferentSuffix);
    assertStreqFails(__LINE__, "a\0b", "a\0B");
    assertStreqFails(__LINE__, strWithNulInside, strWithNulInsideAndDifferentSuffix);
    assertStreqFails(__LINE__, strWithNulInside, "a\0B");
}

TEST(utils, universalString)
{
    using detail::UniversalString;

    // NOTE: ASSERT_STREQ() itself uses UniversalString type for its arguments.

    { // Copy constructor.
        const UniversalString src("text");
        const UniversalString dst(src);
        ASSERT_STREQ("text", dst.s);
    }
    { // From null char*.
        const UniversalString str((const char*) nullptr);
        ASSERT_TRUE(str.isNull);
        ASSERT_STREQ("", str.s);
        ASSERT_STREQ("null", str.toString());
    }
    { // From char array, empty.
        const UniversalString str("");
        ASSERT_FALSE(str.isNull);
        ASSERT_STREQ("", str);
        ASSERT_STREQ("\"\"", str.toString());
    }
    { // From char array, with '\0' inside.
        const UniversalString str("a\0b");
        ASSERT_FALSE(str.isNull);
        ASSERT_EQ(3, (int) str.s.size());
        ASSERT_STREQ("a\000b", str);
        ASSERT_STREQ("\"a\\000b\"", str.toString());
    }
    { // From std::string, empty.
        const UniversalString str(std::string(""));
        ASSERT_FALSE(str.isNull);
        ASSERT_STREQ("", str);
        ASSERT_STREQ("\"\"", str.toString());
    }
    { // From std::string, with '\0' inside.
        const UniversalString str(std::string("a\0b", 3));
        ASSERT_FALSE(str.isNull);
        ASSERT_EQ(3, (int) str.s.size());
        ASSERT_STREQ("a\000b", str);
        ASSERT_STREQ("\"a\\000b\"", str.toString());
    }
    { // From char*.
        const UniversalString str((const char*) "abc");
        ASSERT_FALSE(str.isNull);
        ASSERT_STREQ("abc", str);
        ASSERT_STREQ("\"abc\"", str.toString());
    }
}

} // namespace test
} // namespace test
} // namespace kit
} // namespace nx
