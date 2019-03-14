#include <gtest/gtest.h>

#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>
#include <nx/network/http/http_client.h>
#include <nx/network/ssl/ssl_engine.h>
#include <nx/network/url/url_builder.h>

#include <nx/cloud/mediator/mediator_service.h>

#include "api_test_fixture.h"

namespace nx::hpm::test {

class Https:
    public ApiTestFixture<nx::network::stun::AsyncClientWithHttpTunneling>
{
public:
    Https()
    {
        addArg("--https/listenOn=127.0.0.1:0");
    }

protected:
    virtual nx::utils::Url stunApiUrl() const override
    {
        return baseHttpsUrl();
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
    public ApiTestFixture<nx::network::stun::AsyncClientWithHttpTunneling>
{
public:
    Http()
    {
        addArg("--http/connectionInactivityTimeout=100ms");
        addArg("--stun/connectionInactivityTimeout=100ms");

        if (m_connection)
            m_connection->pleaseStopSync();
    }

protected:
    virtual nx::utils::Url stunApiUrl() const override
    {
        return nx::network::url::Builder(httpUrl()).appendPath(api::kStunOverHttpTunnelPath);
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

TEST_F(Http, malformed_request_is_properly_handled)
{
    whenSendMalformedRequest();

    thenReponseIsReceived();
    andResponseCodeIs(api::ResultCode::badRequest);
}

TEST_F(Http, inactive_upgraded_connection_is_closed_by_timeout)
{
    whenOpenConnectionToMediator();
    thenConnectSucceeded();

    waitForConnectionIsClosedByMediator();
}

} // namespace nx::hpm::test
