// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

// TODO: #akolesnikov Remove this file when <charconv> is supported by all platforms.

#include <string_view>

#include <gtest/gtest.h>

#include <nx/utils/std/charconv.h>

namespace nx::utils::charconv::test {

class CharconvFromChars:
    public ::testing::Test
{
protected:
    template<typename T, typename... Args>
    void assertConverted(const std::string_view& str, T expected, Args... args)
    {
        T val = 0;
        const auto result = from_chars(str.data(), str.data() + str.size(), val, args...);
        ASSERT_EQ(std::errc(), result.ec);
        ASSERT_EQ(expected, val);
        ASSERT_EQ(str.size(), result.ptr - str.data());
    }

    template<typename T, typename... Args>
    void assertInvalidString(const std::string_view& str, Args... args)
    {
        T val = 0;
        const auto result = from_chars(str.data(), str.data() + str.size(), val, args...);
        ASSERT_EQ(std::errc::invalid_argument, result.ec);
        ASSERT_EQ(str.data(), result.ptr);
    }
};

TEST_F(CharconvFromChars, integers)
{
    assertConverted("123456", 123456l, 10);
    assertConverted("1234567890123456", 1234567890123456ll, 10);
    assertConverted("1234567890123456", 1234567890123456ull, 10);
    assertConverted("-1234567890123456", -1234567890123456ll, 10);
}

// TODO: #akolesnikov It appears that gcc implemenation has a bug and does not pass the following test.
// TEST_F(CharconvFromChars, skips_preceding_spaces)
// {
//     assertConverted("   1234567890123456", 1234567890123456ull, 10);
// }

TEST_F(CharconvFromChars, hex)
{
    assertConverted("462D53C8ABAC0", 1234567890123456ull, 16);
}

TEST_F(CharconvFromChars, invalid_string)
{
    assertInvalidString<int>("abcd", 10);
    assertInvalidString<int>("    ", 10);
    assertInvalidString<int>("", 10);
}

TEST_F(CharconvFromChars, float_number)
{
    assertConverted("3.1415926", 3.1415926);
    assertConverted("-3.1415926", (double) -3.1415926);
    assertConverted("1.2e1", 12.0);
}

//-------------------------------------------------------------------------------------------------

class CharconvToChars:
    public ::testing::Test
{
protected:
    template<typename T, typename... Args>
    void assertConverted(const std::string_view& str, T value, Args... args)
    {
        char buf[32];
        const auto result = to_chars(buf, buf + sizeof(buf), value, args...);
        ASSERT_EQ(std::errc(), result.ec);
        ASSERT_EQ(str, std::string_view(buf, result.ptr - buf));
        ASSERT_EQ(result.ptr, buf + str.size());
    }
};

TEST_F(CharconvToChars, integer)
{
    assertConverted("183871", 183871, 10);
    assertConverted("1234567890123456", 1234567890123456ull, 10);
    assertConverted("-1234567890123456", -1234567890123456ll, 10);
}

TEST_F(CharconvToChars, hex)
{
    assertConverted("2ce3f", 183871, 16);
    assertConverted("462d53c8abac0", 1234567890123456ull, 16);
}

TEST_F(CharconvToChars, insufficient_buffer_space)
{
    char buf[3];
    const int value = 183871;
    const auto result = to_chars(buf, buf + sizeof(buf), value, 10);
    ASSERT_EQ(std::errc::value_too_large, result.ec);
    ASSERT_EQ(result.ptr, buf + sizeof(buf));
}

#if defined(NX_CHARCONV_TO_FLOAT)
TEST_F(CharconvToChars, float_number)
{
    assertConverted("3.141592", (float) 3.141592, chars_format::general, 7);
    assertConverted("3.14", 3.1415926, chars_format::general, 3);
    assertConverted("-3.1415926", (double) -3.1415926, chars_format::general, 8);
    assertConverted("12", 12.0, chars_format::general, 10);
}
#endif

} // namespace nx::utils::charconv::test
