// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "enum_utils_ut.h"

#include <gtest/gtest.h>

#include <nx/utils/enum_utils.h>

#define str(s) QString(s)

namespace nx::utils {

namespace test {

TEST(EnumUtils, enumToShortString)
{
    ASSERT_EQ(enumToShortString(EnumUtilsTest::EnumType::Value1), str("Value1"));
    ASSERT_EQ(enumToShortString(EnumUtilsTest::EnumClassType::value1), str("value1"));
    ASSERT_EQ(enumToShortString(EnumUtilsTest::EnumFlag::Flag2), str("Flag2"));
    ASSERT_EQ(enumToShortString(EnumUtilsTest::EnumClassFlag::flag2), str("flag2"));
    ASSERT_EQ(enumToShortString(enum_utils_test::EnumType::Value2), str("Value2"));
    ASSERT_EQ(enumToShortString(enum_utils_test::EnumClassType::value2), str("value2"));
    ASSERT_EQ(enumToShortString(enum_utils_test::EnumFlag::Flag1), str("Flag1"));
    ASSERT_EQ(enumToShortString(enum_utils_test::EnumClassFlag::flag1), str("flag1"));
}

TEST(EnumUtils, enumToString)
{
    ASSERT_EQ(enumToString(EnumUtilsTest::EnumType::Value1), str("EnumType::Value1"));
    ASSERT_EQ(enumToString(EnumUtilsTest::EnumClassType::value1), str("EnumClassType::value1"));
    ASSERT_EQ(enumToString(EnumUtilsTest::EnumFlag::Flag2), str("EnumFlag::Flag2"));
    ASSERT_EQ(enumToString(EnumUtilsTest::EnumClassFlag::flag2), str("EnumClassFlag::flag2"));
    ASSERT_EQ(enumToString(enum_utils_test::EnumType::Value2), str("EnumType::Value2"));
    ASSERT_EQ(enumToString(enum_utils_test::EnumClassType::value2), str("EnumClassType::value2"));
    ASSERT_EQ(enumToString(enum_utils_test::EnumFlag::Flag1), str("EnumFlag::Flag1"));
    ASSERT_EQ(enumToString(enum_utils_test::EnumClassFlag::flag1), str("EnumClassFlag::flag1"));
}

TEST(EnumUtils, enumToFullString)
{
    ASSERT_EQ(enumToFullString(EnumUtilsTest::EnumType::Value1),
        str("nx::utils::test::EnumUtilsTest::EnumType::Value1"));

    ASSERT_EQ(enumToFullString(EnumUtilsTest::EnumClassType::value1),
        str("nx::utils::test::EnumUtilsTest::EnumClassType::value1"));

    ASSERT_EQ(enumToFullString(EnumUtilsTest::EnumFlag::Flag2),
        str("nx::utils::test::EnumUtilsTest::EnumFlag::Flag2"));

    ASSERT_EQ(enumToFullString(EnumUtilsTest::EnumClassFlag::flag2),
        str("nx::utils::test::EnumUtilsTest::EnumClassFlag::flag2"));

    ASSERT_EQ(enumToFullString(enum_utils_test::EnumType::Value2),
        str("nx::utils::test::enum_utils_test::EnumType::Value2"));

    ASSERT_EQ(enumToFullString(enum_utils_test::EnumClassType::value2),
        str("nx::utils::test::enum_utils_test::EnumClassType::value2"));

    ASSERT_EQ(enumToFullString(enum_utils_test::EnumFlag::Flag1),
        str("nx::utils::test::enum_utils_test::EnumFlag::Flag1"));

    ASSERT_EQ(enumToFullString(enum_utils_test::EnumClassFlag::flag1),
        str("nx::utils::test::enum_utils_test::EnumClassFlag::flag1"));
}

} // namespace test
} // namespace nx::utils
