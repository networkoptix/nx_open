// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/reflect/json.h>
#include <nx/utils/deprecated_settings.h>
#include <nx/utils/random.h>
#include <nx/utils/uuid.h>

#include <nx/sql/types.h>

namespace nx::sql::test {

constexpr int argc = 3;
const char* args[] = {
    "/path/to/bin",
    "-db/encoding", "invalidEncoding"
};

class DbConnectionOptions:
    public ::testing::Test
{
public:
    DbConnectionOptions()
    {
        m_defaultOptions.driverType = RdbmsDriverType::postgresql;
        m_defaultOptions.maxConnectionCount = nx::utils::random::number<int>(1, 100);
        m_defaultOptions.dbName = nx::Uuid::createUuid().toSimpleString();
        m_defaultOptions.encoding = "invalidEncoding";
    }

protected:
    void givenOptionsWithCustomDefaultValues()
    {
        m_optionsUnderTest = m_defaultOptions;
    }

    void havingLoadedOptionsFromSettings()
    {
        QnSettings settings("company_name", "app", "mod");
        settings.parseArgs(argc, args);
        m_optionsUnderTest.loadFromSettings(settings);
    }

    void assertIfNotOnlySpecifiedFieldsWereOverridden()
    {
        ASSERT_EQ(m_defaultOptions, m_optionsUnderTest);
    }

private:
    ConnectionOptions m_defaultOptions;
    ConnectionOptions m_optionsUnderTest;
};

TEST_F(DbConnectionOptions, custom_default_values_are_not_overwritten)
{
    givenOptionsWithCustomDefaultValues();
    havingLoadedOptionsFromSettings();
    assertIfNotOnlySpecifiedFieldsWereOverridden();
}

TEST(DbConnectionOptionsSerialization, password_is_not_serialized)
{
    ConnectionOptions options;
    options.userName = "dbUser42";
    options.password = "S3cretDbPa55";

    const auto serialized = nx::reflect::json::serialize(options);

    ASSERT_NE(std::string::npos, serialized.find("dbUser42"));
    ASSERT_EQ(std::string::npos, serialized.find("S3cretDbPa55"));
    ASSERT_EQ(std::string::npos, serialized.find("\"password\""));
}

} // namespace nx::sql::test
