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

static constexpr char kMaintenanceHtdigestPathArg[] = "--http/maintenanceHtdigestPath=";
static constexpr char kHtdigestFile[] = "/htdigest.txt";

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
        this->m_httpClient.setMessageBodyReadTimeout(nx::network::kNoTimeout);
        this->m_httpClient.setResponseReadTimeout(nx::network::kNoTimeout);
    }

    void givenNonEmptyHtdigestFile()
    {
        this->loadHtdigestFile(/*writeCredentials*/ true);
        ASSERT_TRUE(this->startAndWaitUntilStarted());
    }

    void givenNoHtdigestFile()
    {
        ASSERT_TRUE(this->startAndWaitUntilStarted());
    }

    void givenEmptyHtdigestFile()
    {
        this->loadHtdigestFile(/*writeCredentials*/ false);
        ASSERT_TRUE(this->startAndWaitUntilStarted());
    }

    void whenRequestMallocInfoWithValidCredentials()
    {
        this->setClientCredentials(kValidUserAndPassword);
        ASSERT_TRUE(this->m_httpClient.doGet(this->requestUrl(
            nx::network::maintenance::kMallocInfo)));
    }

    void whenRequestLoggingConfigurationWithValidCredentials()
    {
        this->setClientCredentials(kValidUserAndPassword);
        ASSERT_TRUE(this->m_httpClient.doGet(this->requestUrl(
            this->loggerPath())));
    }

    void whenRequestMallocInfoWithAnyCredentials()
    {
        this->whenRequestMallocInfoWithValidCredentials();
    }

    void whenRequestLoggingConfigurationWithAnyCredentials()
    {
        this->whenRequestLoggingConfigurationWithValidCredentials();
    }

    void whenRequestMallocInfoWithInvalidCredentials()
    {
        this->setClientCredentials(kInvalidUserAndPassword);
        ASSERT_TRUE(this->m_httpClient.doGet(this->requestUrl(
            nx::network::maintenance::kMallocInfo)));
    }

    void whenRequestLoggingConfigurationWithInvalidCredentials()
    {
        this->setClientCredentials(kInvalidUserAndPassword);
        ASSERT_TRUE(this->m_httpClient.doGet(this->requestUrl(this->loggerPath())));
    }

    void thenRequestSucceeded(nx::network::http::StatusCode::Value statusCode)
    {
        ASSERT_NE(nullptr, this->m_httpClient.response());
        ASSERT_EQ(statusCode, this->m_httpClient.response()->statusLine.statusCode);
    }

    void thenRequestFailed(nx::network::http::StatusCode::Value statusCode)
    {
        ASSERT_NE(nullptr, this->m_httpClient.response());
        ASSERT_EQ(statusCode, this->m_httpClient.response()->statusLine.statusCode);
    }

private:
    void setClientCredentials(const std::pair<const char*, const char*>& userAndPassword)
    {
        this->m_httpClient.setCredentials(nx::network::http::Credentials(
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
            .setPath(MaintenanceTypeSet::apiPrefix)
            .appendPath(nx::network::maintenance::kMaintenance).appendPath(requestPath);
    }

    void loadHtdigestFile(bool writeCredentials)
    {
        // Give a file path and write credentials.
        std::string htdigestPath = this->testDataDir().toStdString() + kHtdigestFile;
        if (writeCredentials)
        {
            ASSERT_TRUE(this->writeCredentials(htdigestPath));
        }
        this->addArg((std::string(kMaintenanceHtdigestPathArg) + htdigestPath).c_str());
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

private:
    nx::network::http::HttpClient m_httpClient;
};

TYPED_TEST_CASE_P(MaintenanceServiceAcceptance);

TYPED_TEST_P(
    MaintenanceServiceAcceptance,
    malloc_info_is_available_with_valid_credentials_with_non_empty_htdigest_file)
{
    this->givenNonEmptyHtdigestFile();
    this->whenRequestMallocInfoWithValidCredentials();

    this->thenRequestSucceeded(nx::network::http::StatusCode::ok);
}

TYPED_TEST_P(
    MaintenanceServiceAcceptance,
    log_server_is_available_with_valid_credentials_with_non_empty_htdigest_file)
{
    this->givenNonEmptyHtdigestFile();
    this->whenRequestLoggingConfigurationWithValidCredentials();

    this->thenRequestSucceeded(nx::network::http::StatusCode::ok);
}

TYPED_TEST_P(
    MaintenanceServiceAcceptance,
    malloc_info_is_not_available_with_invalid_credentials_with_non_empty_htdigest_file)
{
    this->givenNonEmptyHtdigestFile();
    this->whenRequestMallocInfoWithInvalidCredentials();

    this->thenRequestFailed(nx::network::http::StatusCode::unauthorized);
}

TYPED_TEST_P(
    MaintenanceServiceAcceptance,
    log_server_is_not_available_with_invalid_credentials_with_non_empty_htdigest_file)
{
    this->givenNonEmptyHtdigestFile();
    this->whenRequestLoggingConfigurationWithInvalidCredentials();

    this->thenRequestFailed(nx::network::http::StatusCode::unauthorized);
}

TYPED_TEST_P(
    MaintenanceServiceAcceptance,
    malloc_info_is_available_with_no_htdigest_file)
{
    this->givenNoHtdigestFile();
    this->whenRequestMallocInfoWithAnyCredentials();

    this->thenRequestSucceeded(nx::network::http::StatusCode::ok);
}

TYPED_TEST_P(
    MaintenanceServiceAcceptance,
    log_server_is_available_with_no_htdigest_file)
{
    this->givenNoHtdigestFile();
    this->whenRequestLoggingConfigurationWithAnyCredentials();

    this->thenRequestSucceeded(nx::network::http::StatusCode::ok);
}

TYPED_TEST_P(
    MaintenanceServiceAcceptance,
    malloc_info_is_not_available_with_empty_htdigest_file)
{
    this->givenEmptyHtdigestFile();
    this->whenRequestMallocInfoWithAnyCredentials();

    this->thenRequestFailed(nx::network::http::StatusCode::unauthorized);
}

TYPED_TEST_P(
    MaintenanceServiceAcceptance,
    log_server_is_not_available_with_empty_htdigest_file)
{
    this->givenEmptyHtdigestFile();
    this->whenRequestLoggingConfigurationWithAnyCredentials();

    this->thenRequestFailed(nx::network::http::StatusCode::unauthorized);
}

REGISTER_TYPED_TEST_CASE_P(
    MaintenanceServiceAcceptance,
    malloc_info_is_available_with_valid_credentials_with_non_empty_htdigest_file,
    log_server_is_available_with_valid_credentials_with_non_empty_htdigest_file,
    malloc_info_is_not_available_with_invalid_credentials_with_non_empty_htdigest_file,
    log_server_is_not_available_with_invalid_credentials_with_non_empty_htdigest_file,
    malloc_info_is_available_with_no_htdigest_file,
    log_server_is_available_with_no_htdigest_file,
    malloc_info_is_not_available_with_empty_htdigest_file,
    log_server_is_not_available_with_empty_htdigest_file);

} // namespace nx::network::test