#pragma once

#include <gtest/gtest.h>

#include "functional_tests/mediator_functional_test.h"

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

template<typename MaintenanceTypeSet>
class MaintenanceServiceAcceptance:
    public MaintenanceTypeSet::BaseType
{
protected:
    virtual void SetUp() override
    {
        QString htdigestPath = testDataDir() + "/htdigest.txt";
        ASSERT_TRUE(writeCredentials(htdigestPath));

        addArg(lm("--http/maintenanceHtdigestPath=%1").arg(htdigestPath).toUtf8().constData());

        ASSERT_TRUE(startAndWaitUntilStarted());

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
        return nx::network::url::Builder().setEndpoint(httpEndpoint())
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

    bool writeCredentials(const QString& filePath)
    {
        std::ofstream output(filePath.toStdString(), std::ofstream::out);
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