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

TEST(MysqlErrorText, row_values_are_redacted)
{
    ASSERT_EQ("Duplicate entry '***' for key 'account.email' QMYSQL3: Unable to execute statement",
        detail::redactMysqlErrorValues("Duplicate entry 'john@example.com' for key "
                                       "'account.email' QMYSQL3: Unable to execute statement"));
    ASSERT_EQ("Incorrect string value: '***' for column 'full_name' at row 1",
        detail::redactMysqlErrorValues(
            "Incorrect string value: '\\xF0\\x9F\\x98\\x80' for column 'full_name' at row 1"));
    ASSERT_EQ("Truncated incorrect DOUBLE value: '***' QMYSQL: Unable to execute query",
        detail::redactMysqlErrorValues(
            "Truncated incorrect DOUBLE value: 'john' QMYSQL: Unable to execute query"));
    ASSERT_EQ("Truncated incorrect DOUBLE value: '***'",
        detail::redactMysqlErrorValues("Truncated incorrect DOUBLE value: 'john'"));
    ASSERT_EQ("Invalid utf8mb4 character string: '***'",
        detail::redactMysqlErrorValues("Invalid utf8mb4 character string: 'F09F98'"));
    ASSERT_EQ("Cannot convert string '***' from binary to utf8mb4",
        detail::redactMysqlErrorValues("Cannot convert string '\\xF0' from binary to utf8mb4"));
}

TEST(MysqlErrorText, values_with_quotes_and_newlines_are_redacted)
{
    ASSERT_EQ("Duplicate entry '***' for key 'account.email'",
        detail::redactMysqlErrorValues(
            "Duplicate entry 'o'brien@example.com' for key 'account.email'"));
    ASSERT_EQ("Duplicate entry '***' for key 'system.name'",
        detail::redactMysqlErrorValues("Duplicate entry 'line1\nline2' for key 'system.name'"));
}

TEST(MysqlErrorText, text_without_row_values_is_kept)
{
    ASSERT_EQ("Table 'cdb.account' doesn't exist QMYSQL: Unable to execute query",
        detail::redactMysqlErrorValues(
            "Table 'cdb.account' doesn't exist QMYSQL: Unable to execute query"));
}

} // namespace nx::sql::test
