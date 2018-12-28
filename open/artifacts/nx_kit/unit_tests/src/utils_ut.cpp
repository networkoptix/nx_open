// Copyright 2018-present Network Optix, Inc.

#include <algorithm>
#include <cstring>

#include <nx/kit/test.h>
#include <nx/kit/utils.h>

namespace nx {
namespace kit {
namespace utils {
namespace test {

TEST(utils, unalignedPtr)
{
    char data[1024];
    uint8_t* ptr = unalignedPtr(&data);
    intptr_t ptrInt = (intptr_t) ptr;
    ASSERT_TRUE(ptrInt % 32 != 0);
}

TEST(utils, toString_string)
{
    ASSERT_STREQ("\"abc\"", toString(std::string("abc")));
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
}

TEST(utils, toString_uint8_t)
{
    ASSERT_STREQ("42", toString((uint8_t) 42));
}

TEST(utils, toString_char)
{
    ASSERT_STREQ("'C'", toString('C'));
    ASSERT_STREQ(R"('\'')", toString('\''));
    ASSERT_STREQ(R"('\xFF')", toString('\xFF'));
}

TEST(utils, toString_char_ptr)
{
    ASSERT_STREQ("\"str\"", toString("str"));
    ASSERT_STREQ("\"str\\\"with_quote\"", toString("str\"with_quote"));
    ASSERT_STREQ(R"("str\\with_backslash")", toString("str\\with_backslash"));
    ASSERT_STREQ(R"("str\twith_tab")", toString("str\twith_tab"));
    ASSERT_STREQ(R"("str\nwith_newline")", toString("str\nwith_newline"));
    ASSERT_STREQ(R"("str\x7Fwith_127")", toString("str\x7Fwith_127"));
    ASSERT_STREQ(R"("str\x1Fwith_31")", toString("str\x1Fwith_31"));
    ASSERT_STREQ(R"("str\xFFwith_255")", toString("str\xFFwith_255"));
}

} // namespace test
} // namespace utils
} // namespace kit
} // namespace nx
