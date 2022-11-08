// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/sql/utils.h>

namespace nx::sql::test {

TEST(DbVersion, parse)
{
    ASSERT_EQ(nx::utils::SoftwareVersion(3, 11), detail::parseVersion("3.11"));
    ASSERT_EQ(nx::utils::SoftwareVersion(3, 11, 1), detail::parseVersion("3.11.1"));
    ASSERT_EQ(nx::utils::SoftwareVersion(5, 7, 38), detail::parseVersion("5.7.38-log"));
    ASSERT_EQ(nx::utils::SoftwareVersion(5, 7), detail::parseVersion("5.7log"));

    ASSERT_EQ(std::nullopt, detail::parseVersion("3"));
    ASSERT_EQ(std::nullopt, detail::parseVersion(""));
    ASSERT_EQ(std::nullopt, detail::parseVersion("abcdef"));
}

} // namespace nx::sql::test
