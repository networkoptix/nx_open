// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <algorithm>
#include <climits>

#include <nx/kit/test.h>
#include <nx/kit/utils.h>

namespace nx {
namespace kit {
namespace utils {
namespace test {

TEST(utils, getProcessCmdLineArgs)
{
    const auto& args = getProcessCmdLineArgs();

    ASSERT_FALSE(args.empty());
    ASSERT_TRUE(args[0].find("nx_kit_ut") != std::string::npos);

    std::cerr << "Process has " << args.size() << " arg(s):" << std::endl;
    for (const auto& arg: args)
        std::cerr << nx::kit::utils::toString(arg) << std::endl;
}

TEST(utils, baseName)
{
    ASSERT_STREQ("", baseName(""));
    ASSERT_STREQ("", baseName("/"));
    ASSERT_STREQ("bar", baseName("/foo/bar"));
    ASSERT_STREQ("bar", baseName("foo/bar"));

    #if defined(_WIN32)
        ASSERT_STREQ("bar", baseName("d:bar"));
        ASSERT_STREQ("bar", baseName("X:\\bar"));
    #endif
}

TEST(utils, getProcessName)
{
    const std::string processName = getProcessName();

    ASSERT_FALSE(processName.empty());
    ASSERT_TRUE(processName.find(".exe") == std::string::npos);
    ASSERT_TRUE(processName.find('/') == std::string::npos);
    ASSERT_TRUE(processName.find('\\') == std::string::npos);
}

TEST(utils, alignUp)
{
    ASSERT_EQ(0U, alignUp(0, 0));
    ASSERT_EQ(17U, alignUp(17, 0));
    ASSERT_EQ(48U, alignUp(48, 16));
    ASSERT_EQ(48U, alignUp(42, 16));
    ASSERT_EQ(7U, alignUp(7, 7));
    ASSERT_EQ(8U, alignUp(7, 8));
}

TEST(utils, misalignedPtr)
{
    uint8_t data[1024];
    const auto aligned = (uint8_t*) alignUp((intptr_t) data, 32);
    ASSERT_EQ(0, (intptr_t) aligned % 32);
    uint8_t* const misaligned = misalignedPtr(data);
    ASSERT_TRUE((intptr_t) misaligned % 32 != 0);
}

TEST(utils, toString_string)
{
    ASSERT_STREQ("\"abc\"", toString(/*rvalue*/ std::string("abc")));

    std::string nonConstString = "abc";
    ASSERT_STREQ("\"abc\"", toString(nonConstString));

    const std::string constString = "abc";
    ASSERT_STREQ("\"abc\"", toString(constString));
}

TEST(utils, toString_ptr)
{
    ASSERT_STREQ("null", toString((const void*) nullptr));
    ASSERT_STREQ("null", toString(nullptr));

    const int dummyInt = 42;
    char expectedS[100];
    snprintf(expectedS, sizeof(expectedS), "%p", &dummyInt);
    ASSERT_EQ(expectedS, toString(&dummyInt));
}

TEST(utils, toString_bool)
{
    ASSERT_STREQ("true", toString(true));
    ASSERT_STREQ("false", toString(false));
}

TEST(utils, toString_number)
{
    ASSERT_STREQ("42", toString(42));
    ASSERT_STREQ("3.14", toString(3.14));

    ASSERT_STREQ("42", toString((uint8_t) 42));
    ASSERT_STREQ("42", toString((int8_t) 42));
    ASSERT_STREQ("42", toString((uint16_t) 42));
    ASSERT_STREQ("42", toString((int16_t) 42));
}

TEST(utils, toString_char)
{
    ASSERT_STREQ("'C'", toString('C'));
    ASSERT_STREQ(R"('\'')", toString('\''));
    ASSERT_STREQ(R"('\xFF')", toString('\xFF'));
}

int kWcharSize = sizeof(wchar_t); //< Not const to suppress the warning about a constant condition.

TEST(utils, toString_wchar)
{
    ASSERT_STREQ("'C'", toString(L'C'));
    ASSERT_STREQ(R"('\'')", toString(L'\''));

    if (kWcharSize == 2) //< E.g. MSVC
    {
        ASSERT_STREQ(R"('\x00FF')", toString(L'\xFF'));
        ASSERT_STREQ(R"('\xABCD')", toString(L'\xABCD'));
    }
    else if (kWcharSize == 4) //< E.g. GCC/Clang
    {
        ASSERT_STREQ(R"('\x000000FF')", toString(L'\xFF'));
        ASSERT_STREQ(R"('\x0000ABCD')", toString(L'\xABCD'));
    }
}

TEST(utils, toString_char_ptr)
{
    ASSERT_STREQ(R"("str")", toString("str"));

    char nonConstChars[] = "str";
    ASSERT_STREQ(R"("str")", toString(nonConstChars));

    // `R"(` is not used here because `\"` does not compile in MSVC 2017.
    ASSERT_STREQ("\"str\\\"with_quote\"", toString("str\"with_quote"));

    ASSERT_STREQ(R"("str\\with_backslash")", toString("str\\with_backslash"));
    ASSERT_STREQ(R"("str\twith_tab")", toString("str\twith_tab"));
    ASSERT_STREQ(R"("str\nwith_newline")", toString("str\nwith_newline"));
    ASSERT_STREQ(R"("str\x7F""with_127")", toString("str\x7Fwith_127"));
    ASSERT_STREQ(R"("str\x1F""with_31")", toString("str\x1Fwith_31"));
    ASSERT_STREQ(R"("str\xFF""with_255")", toString("str\xFFwith_255"));
}

TEST(utils, toString_wchar_ptr)
{
    ASSERT_STREQ(R"("str")", toString(L"str"));

    wchar_t nonConstWchars[] = L"str";
    ASSERT_STREQ(R"("str")", toString(nonConstWchars));

    if (kWcharSize == 2) //< MSVC
    {
        // `R"(` is not used here because `\xABCD` does not compile in MSVC 2017.
        ASSERT_STREQ("\"-\\xABCD\"\"-\"", toString(L"-\xABCD-"));
    }
    else if (kWcharSize == 4) //< Linux
    {
        // `R"(` is not used here because `\xABCD` does not compile in MSVC 2017.
        ASSERT_STREQ("\"-\\x0000ABCD\"\"-\"", toString(L"-\x0000ABCD-"));
    }
}

TEST(utils, fromString_int)
{
    int value = 0;

    ASSERT_TRUE(fromString("-3", &value));
    ASSERT_EQ(-3, value);

    ASSERT_TRUE(fromString("42", &value));
    ASSERT_EQ(42, value);

    ASSERT_TRUE(fromString(toString(INT_MAX), &value));
    ASSERT_EQ(INT_MAX, value);

    ASSERT_TRUE(fromString(toString(INT_MIN), &value));
    ASSERT_EQ(INT_MIN, value);

    ASSERT_FALSE(fromString(/* INT_MAX * 10 */ toString(INT_MAX) + "0", &value));
    ASSERT_FALSE(fromString(/* INT_MIN * 10 */ toString(INT_MIN) + "0", &value));

    ASSERT_FALSE(fromString("text", &value));
    ASSERT_FALSE(fromString("42-some-suffix", &value));
    ASSERT_FALSE(fromString("2.0", &value));
    ASSERT_FALSE(fromString("", &value));
}

TEST(utils, fromString_double)
{
    double value = 0;

    ASSERT_TRUE(fromString("-3", &value));
    ASSERT_EQ(-3.0, value);

    ASSERT_TRUE(fromString("42", &value));
    ASSERT_EQ(42.0, value);

    ASSERT_TRUE(fromString("3.14", &value));
    ASSERT_EQ(3.14, value);

    ASSERT_FALSE(fromString("text", &value));
    ASSERT_FALSE(fromString("42-some-suffix", &value));
    ASSERT_FALSE(fromString("", &value));
}

TEST(utils, fromString_float)
{
    float value = 0;

    ASSERT_TRUE(fromString("-3", &value));
    ASSERT_EQ(-3.0F, value);

    ASSERT_TRUE(fromString("42", &value));
    ASSERT_EQ(42.0F, value);

    ASSERT_TRUE(fromString("3.14", &value));
    ASSERT_EQ(3.14F, value);

    ASSERT_FALSE(fromString("text", &value));
    ASSERT_FALSE(fromString("42-some-suffix", &value));
    ASSERT_FALSE(fromString("", &value));
}

} // namespace test
} // namespace utils
} // namespace kit
} // namespace nx
