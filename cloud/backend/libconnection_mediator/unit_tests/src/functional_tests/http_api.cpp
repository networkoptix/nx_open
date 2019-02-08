#include <gtest/gtest.h>

#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>
#include <nx/network/http/http_client.h>
#include <nx/network/ssl/ssl_engine.h>
#include <nx/network/url/url_builder.h>

#include <nx/cloud/mediator/mediator_service.h>

#include "mediator_functional_test.h"

namespace nx::hpm::test {

class Https:
    public MediatorFunctionalTest
{
protected:
    virtual void SetUp() override
    {
        addArg("--https/listenOn=127.0.0.1:0");

        ASSERT_TRUE(startAndWaitUntilStarted());
    }

    void assertRequestOverHttpsWorks()
    {
        auto url = baseHttpsUrl();
        url.setPath(nx::network::url::joinPath(
            nx::hpm::api::kMediatorApiPrefix,
            nx::hpm::api::kStatisticsMetricsPath).c_str());

        nx::network::http::HttpClient client;
        ASSERT_TRUE(client.doGet(url));
        ASSERT_TRUE(nx::network::http::StatusCode::isSuccessCode(
            client.response()->statusLine.statusCode));
    }

    nx::utils::Url baseHttpsUrl() const
    {
        return nx::network::url::Builder()
            .setScheme(nx::network::http::kSecureUrlSchemeName)
            .setEndpoint(moduleInstance()->impl()->httpsEndpoints().front()).toUrl();
    }
};

//-------------------------------------------------------------------------------------------------

TEST_F(Https, mediator_accepts_https)
{
    assertRequestOverHttpsWorks();
}

//-------------------------------------------------------------------------------------------------

class Http:
    public MediatorFunctionalTest
{
public:
    Http()
    {
        if (m_connection)
            m_connection->pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        addArg("--http/connectionInactivityTimeout=1ms");

        ASSERT_TRUE(startAndWaitUntilStarted());
    }

    void establishTcpConnection()
    {
        m_connection = std::make_unique<nx::network::TCPSocket>(AF_INET);
        ASSERT_TRUE(m_connection->connect(httpEndpoint(), network::kNoTimeout));
    }

    void assertConnectionIsClosedByMediator()
    {
        std::array<char, 64> readBuffer;
        ASSERT_EQ(0, m_connection->recv(readBuffer.data(), readBuffer.size()));
    }

private:
    std::unique_ptr<network::AbstractStreamSocket> m_connection;
};

TEST_F(Http, connection_is_closed_by_inactivity_timer)
{
    establishTcpConnection();
    assertConnectionIsClosedByMediator();
}

} // namespace nx::hpm::test
