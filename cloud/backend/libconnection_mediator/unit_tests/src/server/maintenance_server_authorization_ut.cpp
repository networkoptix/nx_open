#include <gtest/gtest.h>

#include "functional_tests/mediator_functional_test.h"

#include <nx/network/http/http_client.h>
#include <nx/network/http/http_types.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/maintenance/server.h>
#include <nx/network/maintenance/request_path.h>
#include <nx/network/maintenance/log/request_path.h>
#include <nx/cloud/mediator/mediator_service.h>
#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>

namespace nx::hpm::test {

namespace {

static const std::pair<const char*, const char*> kValidUserAndPassword(
    "valid_user",
    "7ab592129c1345326c0cea345e71170a");

static const std::pair<const char*, const char*> kInvalidUserAndPassword("unknown_user", "");

} // namespace

class MaintenanceServerAuthentication:
    public MediatorFunctionalTest
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
            .setPath(nx::hpm::api::kMediatorApiPrefix).
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

TEST_F(MaintenanceServerAuthentication, malloc_info_is_available_with_valid_credentials)
{
    whenRequestMallocInfoWithValidCredentials();

    thenRequestSucceeded(nx::network::http::StatusCode::ok);
}

TEST_F(MaintenanceServerAuthentication, log_server_is_available_with_valid_credentials)
{
    whenRequestLoggingConfigurationWithValidCredentials();

    thenRequestSucceeded(nx::network::http::StatusCode::ok);
}

TEST_F(MaintenanceServerAuthentication, malloc_info_is_not_available_with_invalid_credentials)
{
    whenRequestMallocInfoWithInvalidCredentials();

    thenRequestFailed(nx::network::http::StatusCode::unauthorized);
}

TEST_F(MaintenanceServerAuthentication, log_server_is_not_available_with_invalid_credentials)
{
    whenRequestLoggingConfigurationWithInvalidCredentials();

    thenRequestFailed(nx::network::http::StatusCode::unauthorized);
}


} // namespace nx::hpm::test