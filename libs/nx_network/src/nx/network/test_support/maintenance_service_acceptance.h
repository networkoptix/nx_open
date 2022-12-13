// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/auth_tools.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/http_types.h>
#include <nx/network/maintenance/log/request_path.h>
#include <nx/network/maintenance/request_path.h>
#include <nx/network/maintenance/server.h>
#include <nx/network/socket_factory.h>
#include <nx/network/url/url_builder.h>

namespace nx::network::test {

namespace {

/**
 * NOTE: These credentials were created using htdigest command line using realm "VMS", which MUST
 * match BaseAuthenticationManager::realm().
 */
static constexpr std::tuple<const char*, const char*, const char*> kValidCredentialsHa1{
    "nx",
    "VMS",
    "3aaac5a82ab99e9ebc1d8c43d4a172dc"};

// Correspond to kValidCredentialsHa1 without MD5 hashing
static constexpr std::tuple<const char*, const char*, const char*> kValidCredentialsBasic{
    "nx",
    "VMS",
    "qweasd1234"};

static constexpr std::tuple<const char*, const char*, const char*> kInvalidCredentials{
    "nx",
    "VMS",
    "broken_password"};

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
 * INSTANTIATE_TYPED_TEST_SUITE_P(
 *    SomeTestPrefix,                             //< Can be anything
 *    MaintenanceServiceWithNonEmptyHtdigestFile, //< Do not change
 *    MaintenanceTypeSetImpl);                    //< Must match the struct name above
 * </code>
 *
 * NOTE: SomeParentClass must:
 *    - inherit from testing::Test (from GoogleTest)
 *    - implement `void addArg(const char *)` method
 *    - implement `std::string testDataDir() const` method
 *    - implement `bool startAndWaitUntilStarted()` method
 *    - implement `nx::network::SocketAddress httpEndpoint() const` method
 *    - implement `nx::network::SocketAddress httpsEndpoint() const` method
 */
template<typename MaintenanceTypeSet>
class MaintenanceService:
    public MaintenanceTypeSet::BaseType
{
protected:
    virtual void SetUp() override
    {
        this->m_httpClient = std::make_unique<http::HttpClient>(ssl::kAcceptAnyCertificate);
        this->m_httpClient->setMessageBodyReadTimeout(nx::network::kNoTimeout);
        this->m_httpClient->setResponseReadTimeout(nx::network::kNoTimeout);
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
        this->setClientCredentials(kValidCredentialsHa1);
        ASSERT_TRUE(this->m_httpClient->doGet(this->requestUrl(
            nx::network::maintenance::kMallocInfo)));
    }

    void whenRequestWithValidCredentialsOverHttps(
        std::string requestPath,
        std::optional<http::AuthType> auth = std::nullopt)
    {
        m_httpClient = std::make_unique<http::HttpClient>(ssl::kAcceptAnyCertificate);

        const auto& creds = auth && *auth == http::AuthType::authBasic
            ? kValidCredentialsBasic
            : kValidCredentialsHa1;

        this->setClientCredentials(creds, auth);
        ASSERT_TRUE(this->m_httpClient->doGet(this->httpsUrl(requestPath)));
    }

    void whenRequestLoggingConfigurationWithValidCredentials()
    {
        this->setClientCredentials(kValidCredentialsHa1);
        ASSERT_TRUE(this->m_httpClient->doGet(this->requestUrl(
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
        this->setClientCredentials(kInvalidCredentials);
        ASSERT_TRUE(this->m_httpClient->doGet(this->requestUrl(
            nx::network::maintenance::kMallocInfo)));
    }

    void whenRequestLoggingConfigurationWithInvalidCredentials()
    {
        this->setClientCredentials(kInvalidCredentials);
        ASSERT_TRUE(this->m_httpClient->doGet(this->requestUrl(this->loggerPath())));
    }

    void thenRequestSucceeded(nx::network::http::StatusCode::Value statusCode)
    {
        ASSERT_NE(nullptr, this->m_httpClient->response());
        ASSERT_EQ(statusCode, this->m_httpClient->response()->statusLine.statusCode);
    }

    void thenRequestFailed(nx::network::http::StatusCode::Value statusCode)
    {
        ASSERT_NE(nullptr, this->m_httpClient->response());
        ASSERT_EQ(statusCode, this->m_httpClient->response()->statusLine.statusCode);
    }

    void setClientCredentials(
        const std::tuple<const char*, const char*, const char*>& credentials,
        std::optional<http::AuthType> auth = std::nullopt)
    {
        http::Credentials creds;
        creds.username = std::get<0>(credentials);
        creds.authToken.value = std::get<2>(credentials);
        creds.authToken.type = auth && *auth == http::AuthType::authBasic
            ? http::AuthTokenType::password
            : http::AuthTokenType::ha1;

        this->m_httpClient->setCredentials(creds);

        if (auth)
            this->m_httpClient->setAuthType(*auth);
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

    nx::utils::Url httpsUrl(const std::string& requestPath) const
    {
        return nx::network::url::Builder()
            .setScheme(nx::network::http::kSecureUrlSchemeName)
            .setEndpoint(this->httpsEndpoint())
            .setPath(MaintenanceTypeSet::apiPrefix)
            .appendPath(nx::network::maintenance::kMaintenance)
            .appendPath(requestPath);
    }

    void loadHtdigestFile(bool writeCredentials)
    {
        // Give a file path and write credentials.
        std::string htdigestPath = this->testDataDir().toStdString() + kHtdigestFile;
        if (writeCredentials)
        {
            ASSERT_TRUE(this->writeCredentials(htdigestPath));
        }
        this->addArg((kMaintenanceHtdigestPathArg + htdigestPath).c_str());
    }

    bool writeCredentials(const std::string& filePath) const
    {
        std::ofstream output(filePath, std::ofstream::out);
        if (!output.is_open())
            return false;

        const auto& [username, realm, password] = kValidCredentialsHa1;
        output << username << ":" << realm << ":" << password << std::endl;

        output.close();
        return true;
    }

protected:
    std::unique_ptr<nx::network::http::HttpClient> m_httpClient;
};

TYPED_TEST_SUITE_P(MaintenanceService);

TYPED_TEST_P(
    MaintenanceService,
    malloc_info_is_available_with_valid_credentials)
{
    this->givenNonEmptyHtdigestFile();
    this->whenRequestMallocInfoWithValidCredentials();

    this->thenRequestSucceeded(nx::network::http::StatusCode::ok);
}

TYPED_TEST_P(
    MaintenanceService,
    log_server_is_available_with_valid_credentials)
{
    this->givenNonEmptyHtdigestFile();
    this->whenRequestLoggingConfigurationWithValidCredentials();

    this->thenRequestSucceeded(nx::network::http::StatusCode::ok);
}

TYPED_TEST_P(
    MaintenanceService,
    malloc_info_is_forbidden_with_invalid_credentials)
{
    this->givenNonEmptyHtdigestFile();
    this->whenRequestMallocInfoWithInvalidCredentials();

    this->thenRequestFailed(nx::network::http::StatusCode::unauthorized);
}

TYPED_TEST_P(
    MaintenanceService,
    log_server_is_forbidden_with_invalid_credentials)
{
    this->givenNonEmptyHtdigestFile();
    this->whenRequestLoggingConfigurationWithInvalidCredentials();

    this->thenRequestFailed(nx::network::http::StatusCode::unauthorized);
}

TYPED_TEST_P(
    MaintenanceService,
    malloc_info_is_available_with_no_htdigest_file)
{
    this->givenNoHtdigestFile();
    this->whenRequestMallocInfoWithAnyCredentials();

    this->thenRequestSucceeded(nx::network::http::StatusCode::ok);
}

TYPED_TEST_P(
    MaintenanceService,
    log_server_is_available_with_no_htdigest_file)
{
    this->givenNoHtdigestFile();
    this->whenRequestLoggingConfigurationWithAnyCredentials();

    this->thenRequestSucceeded(nx::network::http::StatusCode::ok);
}

TYPED_TEST_P(
    MaintenanceService,
    malloc_info_is_forbidden_with_empty_htdigest_file)
{
    this->givenEmptyHtdigestFile();
    this->whenRequestMallocInfoWithAnyCredentials();

    this->thenRequestFailed(nx::network::http::StatusCode::unauthorized);
}

TYPED_TEST_P(
    MaintenanceService,
    log_server_is_forbidden_with_empty_htdigest_file)
{
    this->givenEmptyHtdigestFile();
    this->whenRequestLoggingConfigurationWithAnyCredentials();

    this->thenRequestFailed(nx::network::http::StatusCode::unauthorized);
}

TYPED_TEST_P(
    MaintenanceService,
    maintenance_paths_available_over_https_with_basic_auth)
{
    this->givenNonEmptyHtdigestFile();

    const std::vector<std::string> requestPaths{
        maintenance::kMallocInfo,
        this->loggerPath(),
        maintenance::kHealth,
        maintenance::kVersion};

    for (const auto& requestPath: requestPaths)
    {
        this->whenRequestWithValidCredentialsOverHttps(
            requestPath,
            nx::network::http::AuthType::authBasic);

        this->thenRequestSucceeded(nx::network::http::StatusCode::ok);
    }
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(MaintenanceService);
REGISTER_TYPED_TEST_SUITE_P(
    MaintenanceService,
    malloc_info_is_available_with_valid_credentials,
    log_server_is_available_with_valid_credentials,
    malloc_info_is_forbidden_with_invalid_credentials,
    log_server_is_forbidden_with_invalid_credentials,
    malloc_info_is_available_with_no_htdigest_file,
    log_server_is_available_with_no_htdigest_file,
    malloc_info_is_forbidden_with_empty_htdigest_file,
    log_server_is_forbidden_with_empty_htdigest_file,
    maintenance_paths_available_over_https_with_basic_auth);

//-------------------------------------------------------------------------------------------------
// MaintenanceServiceHealth

/** This test is specific to services that use the maintenance server with authentication but need
 * to ensure that {basePath}/maintenance/health does not require authentication. */
template<typename MaintenanceTypeSet>
class MaintenanceServiceHealth:
    public MaintenanceService<MaintenanceTypeSet>
{
protected:
    void whenRequestHealthWithoutCredentials()
    {
        ASSERT_TRUE(this->m_httpClient->doGet(this->requestUrl(
            nx::network::maintenance::kHealth)));
    }
};

TYPED_TEST_SUITE_P(MaintenanceServiceHealth);

TYPED_TEST_P(
    MaintenanceServiceHealth,
    health_is_available_with_non_empty_htdigest_file)
{
    this->givenNonEmptyHtdigestFile();
    this->whenRequestHealthWithoutCredentials();

    this->thenRequestSucceeded(nx::network::http::StatusCode::ok);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(MaintenanceServiceHealth);
REGISTER_TYPED_TEST_SUITE_P(MaintenanceServiceHealth,
    health_is_available_with_non_empty_htdigest_file);

} // namespace nx::network::test
