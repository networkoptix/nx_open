#pragma once

#include <nx/network/http/http_client.h>
#include <nx/network/http/http_types.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/maintenance/server.h>
#include <nx/network/maintenance/request_path.h>
#include <nx/network/maintenance/log/request_path.h>

namespace nx::network::test {

namespace {

static const std::pair<const char*, const char*> kValidUserAndPassword(
    "valid_user",
    "7ab592129c1345326c0cea345e71170a");

static const std::pair<const char*, const char*> kInvalidUserAndPassword("unknown_user", "");

} // namespace

/**
 * GTest template for maintenance server authentication tests.
 *
 * In unit_test.cpp file, put the following:
 *
 * <code>
 * struct MaintenanceTypeSetImpl
 * {
 *     static const char* apiPrefix;
 *     using BaseType = SomeParentClass;
 * };
 *
 * using namespace nx::network::test;
 * INSTANTIATE_TYPED_TEST_CASE_P(
 *    SomeTestPrefix,                                       //< Can be anything
 *    MaintenanceServiceAcceptanceWithNonEmptyHtdigestFile, //< Do not change
 *    MaintenanceTypeSetImpl);                              //< Must match the struct name above

 INSTANTIATE_TYPED_TEST_CASE_P(
 *    SomeTestPrefix,                                 //< Can be anything
 *    MaintenanceServiceAcceptanceWithNoHtdigestFile, //< Do not change
 *    MaintenanceTypeSetImplementation);              //< Must match the struct name above

 INSTANTIATE_TYPED_TEST_CASE_P(
 *    SomeTestPrefix,                                    //< Can be anything
 *    MaintenanceServiceAcceptanceWithEmptyHtdigestFile, //< Do not change
 *    MaintenanceTypeSetImpl);                           //< Must match the struct name above
 * </code>
 *
 * NOTE: SomeParentClass must:
 *    - inherit from testing::Test (from google test)
 *    - implement `void addArg(const char *)` method
 *    - implement `QString testDataDir() const` method
 *    - implement `bool startAndWaitUntilStarted()` method
 *    - implement `nx::network::SocketAddress httpEndpoint() const` method
 */
template<typename MaintenanceTypeSet>
class AbstractMaintenanceServiceAcceptance:
    public MaintenanceTypeSet::BaseType
{
protected:
    virtual void loadHtdigestFile() = 0;

    virtual void SetUp() override
    {
        this->loadHtdigestFile();

        ASSERT_TRUE(this->startAndWaitUntilStarted());

        m_httpClient.setMessageBodyReadTimeout(nx::network::kNoTimeout);
        m_httpClient.setResponseReadTimeout(nx::network::kNoTimeout);
    }

    void whenRequestMallocInfoWithValidCredentials()
    {
        setClientCredentials(kValidUserAndPassword);
        ASSERT_TRUE(m_httpClient.doGet(requestUrl(nx::network::maintenance::kMallocInfo)));
    }

    void whenRequestLoggingConfigurationWithValidCredentials()
    {
        setClientCredentials(kValidUserAndPassword);
        ASSERT_TRUE(m_httpClient.doGet(requestUrl(loggerPath())));
    }

    void whenRequestMallocInfoWithAnyCredentials()
    {
        this->whenRequestMallocInfoWithValidCredentials();
    }

    void whenRequestLoggingConfigurationWithAnyCredentials()
    {
        this->whenRequestLoggingConfigurationWithValidCredentials();
    }

    void thenRequestSucceeded(nx::network::http::StatusCode::Value statusCode)
    {
        ASSERT_NE(nullptr, m_httpClient.response());
        ASSERT_EQ(statusCode, m_httpClient.response()->statusLine.statusCode);
    }

    void thenRequestFailed(nx::network::http::StatusCode::Value statusCode)
    {
        ASSERT_NE(nullptr, m_httpClient.response());
        ASSERT_EQ(statusCode, m_httpClient.response()->statusLine.statusCode);
    }

    void setClientCredentials(const std::pair<const char*, const char*>& userAndPassword)
    {
        m_httpClient.setCredentials(nx::network::http::Credentials(
            userAndPassword.first,
            nx::network::http::Ha1AuthToken(userAndPassword.second)));
    }

    std::string loggerPath() const
    {
        return nx::network::url::joinPath(
            nx::network::maintenance::kLog,
            nx::network::maintenance::log::kLoggers);
    }

    nx::utils::Url requestUrl(const std::string& requestPath) const
    {
        return nx::network::url::Builder().setEndpoint(this->httpEndpoint())
            .setScheme(nx::network::http::kUrlSchemeName)
            .setPath(MaintenanceTypeSet::apiPrefix).
            appendPath(nx::network::maintenance::kMaintenance).appendPath(requestPath);
    }

protected:
    nx::network::http::HttpClient m_httpClient;
};

//-------------------------------------------------------------------------------------------------
// Non empty htdigest file

template<typename MaintenanceTypeSet>
class MaintenanceServiceAcceptanceWithNonEmptyHtdigestFile:
    public AbstractMaintenanceServiceAcceptance<MaintenanceTypeSet>
{
protected:
    void whenRequestMallocInfoWithInvalidCredentials()
    {
        setClientCredentials(kInvalidUserAndPassword);
        ASSERT_TRUE(m_httpClient.doGet(requestUrl(nx::network::maintenance::kMallocInfo)));
    }

    void whenRequestLoggingConfigurationWithInvalidCredentials()
    {
        setClientCredentials(kInvalidUserAndPassword);
        ASSERT_TRUE(m_httpClient.doGet(requestUrl(nx::network::maintenance::kMallocInfo)));
    }

private:
    virtual void loadHtdigestFile() override
    {
        // Give a file path but do not write any credentials.
        std::string htdigestPath = this->testDataDir().toStdString() + "/htdigest.txt";
        ASSERT_TRUE(this->writeCredentials(htdigestPath));

        this->addArg((std::string("--http/maintenanceHtdigestPath=") + htdigestPath).c_str());
    }

    bool writeCredentials(const std::string& filePath) const
    {
        std::ofstream output(filePath, std::ofstream::out);
        if (!output.is_open())
            return false;

        output << kValidUserAndPassword.first << ":users:" << kValidUserAndPassword.second
            << std::endl;

        output.close();
        return true;
    }
};

TYPED_TEST_CASE_P(MaintenanceServiceAcceptanceWithNonEmptyHtdigestFile);

TYPED_TEST_P(
    MaintenanceServiceAcceptanceWithNonEmptyHtdigestFile,
    malloc_info_is_available_with_valid_credentials)
{
    // givenNonEmptyHtdigestFile();
    this->whenRequestMallocInfoWithValidCredentials();

    this->thenRequestSucceeded(nx::network::http::StatusCode::ok);
}

TYPED_TEST_P(
    MaintenanceServiceAcceptanceWithNonEmptyHtdigestFile,
    log_server_is_available_with_valid_credentials)
{
    // givenNonEmptyHtdigestFile();
    this->whenRequestLoggingConfigurationWithValidCredentials();

    this->thenRequestSucceeded(nx::network::http::StatusCode::ok);
}

TYPED_TEST_P(
    MaintenanceServiceAcceptanceWithNonEmptyHtdigestFile,
    malloc_info_is_not_available_with_invalid_credentials)
{
    // givenNonEmptyHtdigestFile();
    this->whenRequestMallocInfoWithInvalidCredentials();

    this->thenRequestFailed(nx::network::http::StatusCode::unauthorized);
}

TYPED_TEST_P(
    MaintenanceServiceAcceptanceWithNonEmptyHtdigestFile,
    log_server_is_not_available_with_invalid_credentials)
{
    // givenNonEmptyHtdigestFile();
    this->whenRequestLoggingConfigurationWithInvalidCredentials();

    this->thenRequestFailed(nx::network::http::StatusCode::unauthorized);
}

REGISTER_TYPED_TEST_CASE_P(
    MaintenanceServiceAcceptanceWithNonEmptyHtdigestFile,
    malloc_info_is_available_with_valid_credentials,
    log_server_is_available_with_valid_credentials,
    malloc_info_is_not_available_with_invalid_credentials,
    log_server_is_not_available_with_invalid_credentials);

//-------------------------------------------------------------------------------------------------
// No htdigest file

template<typename MaintenanceTypeSet>
class MaintenanceServiceAcceptanceWithNoHtdigestFile:
    public AbstractMaintenanceServiceAcceptance<MaintenanceTypeSet>
{
private:
    virtual void loadHtdigestFile() override
    {
        // Do not specify a file path.
    }
};

TYPED_TEST_CASE_P(MaintenanceServiceAcceptanceWithNoHtdigestFile);

TYPED_TEST_P(
    MaintenanceServiceAcceptanceWithNoHtdigestFile,
    malloc_info_is_available)
{
    // givenNoHtdigestFile();
    this->whenRequestMallocInfoWithAnyCredentials();

    this->thenRequestSucceeded(nx::network::http::StatusCode::ok);
}

TYPED_TEST_P(
    MaintenanceServiceAcceptanceWithNoHtdigestFile,
    log_server_is_available)
{
    // givenNoHtdigestFile();
    this->whenRequestLoggingConfigurationWithAnyCredentials();

    this->thenRequestSucceeded(nx::network::http::StatusCode::ok);
}

REGISTER_TYPED_TEST_CASE_P(
    MaintenanceServiceAcceptanceWithNoHtdigestFile,
    malloc_info_is_available,
    log_server_is_available);

//-------------------------------------------------------------------------------------------------
// Empty htdigest file

template<typename MaintenanceTypeSet>
class MaintenanceServiceAcceptanceWithEmptyHtdigestFile:
    public AbstractMaintenanceServiceAcceptance<MaintenanceTypeSet>
{
private:
    virtual void loadHtdigestFile() override
    {
        // Give a file path but do not write any credentials.
        std::string htdigestPath = this->testDataDir().toStdString() + "/htdigest.txt";
        this->addArg((std::string("--http/maintenanceHtdigestPath=") + htdigestPath).c_str());
    }
};

TYPED_TEST_CASE_P(MaintenanceServiceAcceptanceWithEmptyHtdigestFile);

TYPED_TEST_P(
    MaintenanceServiceAcceptanceWithEmptyHtdigestFile,
    malloc_info_is_not_available)
{
    // givenEmptyHtdigestFile();
    this->whenRequestMallocInfoWithAnyCredentials();

    this->thenRequestFailed(nx::network::http::StatusCode::unauthorized);
}

TYPED_TEST_P(
    MaintenanceServiceAcceptanceWithEmptyHtdigestFile,
    log_server_is_not_available)
{
    // givenEmptyHtdigestFile();
    this->whenRequestLoggingConfigurationWithAnyCredentials();

    this->thenRequestFailed(nx::network::http::StatusCode::unauthorized);
}

REGISTER_TYPED_TEST_CASE_P(
    MaintenanceServiceAcceptanceWithEmptyHtdigestFile,
    malloc_info_is_not_available,
    log_server_is_not_available);

} // namespace nx::network::test