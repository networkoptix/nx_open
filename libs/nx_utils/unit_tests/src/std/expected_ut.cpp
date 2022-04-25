// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/std/expected.h>

namespace nx::utils::test {

TEST(StdExpected, with_value)
{
    expected<std::string, int> val("Hello");
    GTEST_ASSERT_TRUE(val);
    GTEST_ASSERT_FALSE(!val);
    GTEST_ASSERT_TRUE(val.has_value());
    GTEST_ASSERT_EQ(std::string("Hello"), *val);
    GTEST_ASSERT_EQ(std::string("Hello"), val.value());
    GTEST_ASSERT_EQ(std::string("Hello").size(), val->size());
}

TEST(StdExpected, with_error)
{
    expected<std::string, int> val(123);
    GTEST_ASSERT_TRUE(!val);
    GTEST_ASSERT_FALSE(val);
    GTEST_ASSERT_FALSE(val.has_value());
    GTEST_ASSERT_EQ(123, val.error());
}

} // namespace nx::utils::test
