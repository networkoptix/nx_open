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
 * struct MaintenanceTypeSetImplementation
 * {
 *     static const char* apiPrefix;
 *     using BaseType = SomeParentClass;
 * };
 *
 * using namespace nx::network::test;
 * INSTANTIATE_TYPED_TEST_CASE_P(
 *    SomeTestPrefix,                    //< Can be anything
 *    MaintenanceServiceAcceptance,      //< Do not change
 *    MaintenanceTypeSetImplementation); //< Must match the struct name above
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
class MaintenanceServiceAcceptance:
    public MaintenanceTypeSet::BaseType
{
protected:
    virtual void SetUp() override
    {
        std::string htdigestPath = this->testDataDir().toStdString() + "/htdigest.txt";
        ASSERT_TRUE(writeCredentials(htdigestPath));

        this->addArg((std::string("--http/maintenanceHtdigestPath=") + htdigestPath).c_str());

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

    void thenRequestSucceeded(nx::network::http::StatusCode::Value statusCode)
    {
        ASSERT_NE(nullptr, m_httpClient.response());
        ASSERT_EQ(statusCode, m_httpClient.response()->statusLine.statusCode);
    }

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

    void thenRequestFailed(nx::network::http::StatusCode::Value statusCode)
    {
        ASSERT_NE(nullptr, m_httpClient.response());
        ASSERT_EQ(statusCode, m_httpClient.response()->statusLine.statusCode);
    }

    std::string loggerPath() const
    {
        return nx::network::url::joinPath(
            nx::network::maintenance::kLog,
            nx::network::maintenance::log::kLoggers);
    }

    nx::utils::Url requestUrl(const std::string& requestPath)
    {
        return nx::network::url::Builder().setEndpoint(this->httpEndpoint())
            .setScheme(nx::network::http::kUrlSchemeName)
            .setPath(MaintenanceTypeSet::apiPrefix).
            appendPath(nx::network::maintenance::kMaintenance).appendPath(requestPath);
    }

    void setClientCredentials(const std::pair<const char*, const char*>& userAndPassword)
    {
        m_httpClient.setCredentials(nx::network::http::Credentials(
            userAndPassword.first,
            nx::network::http::Ha1AuthToken(userAndPassword.second)));
    }

    bool writeCredentials(const std::string& filePath)
    {
        std::ofstream output(filePath, std::ofstream::out);
        if (!output.is_open())
            return false;

        output << kValidUserAndPassword.first << ":users:" << kValidUserAndPassword.second
            << std::endl;

        output.close();
        return true;
    }

private:
    nx::network::maintenance::Server m_maintenanceServer;
    nx::network::http::HttpClient m_httpClient;
};

TYPED_TEST_CASE_P(MaintenanceServiceAcceptance);

TYPED_TEST_P(MaintenanceServiceAcceptance, malloc_info_is_available_with_valid_credentials)
{
    this->whenRequestMallocInfoWithValidCredentials();

    this->thenRequestSucceeded(nx::network::http::StatusCode::ok);
}

TYPED_TEST_P(MaintenanceServiceAcceptance, log_server_is_available_with_valid_credentials)
{
    this->whenRequestLoggingConfigurationWithValidCredentials();

    this->thenRequestSucceeded(nx::network::http::StatusCode::ok);
}

TYPED_TEST_P(MaintenanceServiceAcceptance, malloc_info_is_not_available_with_invalid_credentials)
{
    this->whenRequestMallocInfoWithInvalidCredentials();

    this->thenRequestFailed(nx::network::http::StatusCode::unauthorized);
}

TYPED_TEST_P(MaintenanceServiceAcceptance, log_server_is_not_available_with_invalid_credentials)
{
    this->whenRequestLoggingConfigurationWithInvalidCredentials();

    this->thenRequestFailed(nx::network::http::StatusCode::unauthorized);
}

REGISTER_TYPED_TEST_CASE_P(
    MaintenanceServiceAcceptance,
    malloc_info_is_available_with_valid_credentials,
    log_server_is_available_with_valid_credentials,
    malloc_info_is_not_available_with_invalid_credentials,
    log_server_is_not_available_with_invalid_credentials);

} // namespace nx::network::test