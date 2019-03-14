#include <tuple>

#include <gtest/gtest.h>

#include <nx/network/http/tunneling/detail/client_factory.h>
#include <nx/network/http/tunneling/detail/connection_upgrade_tunnel_client.h>
#include <nx/network/stun/async_client_with_http_tunneling.h>
#include <nx/network/stun/stream_socket_server.h>
#include <nx/network/stun/stun_types.h>
#include <nx/network/test_support/stun_async_client_acceptance_tests.h>
#include <nx/network/test_support/stun_simple_server.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/sync_queue.h>

#include "stun_over_http_server_fixture.h"

namespace nx {
namespace network {
namespace stun {
namespace test {

class AsyncClientWithHttpTunneling:
    public StunOverHttpServerFixture
{
    using base_type = StunOverHttpServerFixture;

public:
    AsyncClientWithHttpTunneling():
        m_client(AbstractAsyncClient::Settings()),
        m_stunServer(&dispatcher(), false)
    {
    }

    ~AsyncClientWithHttpTunneling()
    {
        m_client.pleaseStopSync();

        if (m_tunnelFactoryBak)
        {
            http::tunneling::detail::ClientFactory::instance().setCustomFunc(
                std::move(*m_tunnelFactoryBak));
        }
    }

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        givenTunnelingServer();

        ASSERT_TRUE(m_stunServer.bind(SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(m_stunServer.listen());
    }

    void givenScheduledStunRequest()
    {
        m_client.sendRequest(
            prepareRequest(),
            [this](auto&&... args)
            {
                onResponseReceived(std::forward<decltype(args)>(args)...);
            });
    }

    void givenHttpServerThatAlwaysResponds(http::StatusCode::Value statusCode)
    {
        m_httpServer = std::make_unique<http::TestHttpServer>();
        ASSERT_TRUE(m_httpServer->bindAndListen());
        m_httpServer->registerRequestProcessorFunc(
            http::kAnyPath,
            [statusCode](
                nx::network::http::RequestContext /*requestContext*/,
                nx::network::http::RequestProcessedHandler completionHandler)
            {
                completionHandler(statusCode);
            });
    }

    void whenConnectToStunOverHttpServer()
    {
        whenConnectToHttpServer();
        thenConnectSucceeded();
    }

    void whenConnectToHttpServer()
    {
        m_client.connect(
            httpUrl(),
            [this](SystemError::ErrorCode sysErrorCode)
            {
                m_connectResults.push(sysErrorCode);
            });
    }

    void whenConnectToRegularStunServer()
    {
        nx::utils::promise<SystemError::ErrorCode> done;
        m_client.connect(
            network::url::Builder().setScheme(nx::network::stun::kUrlSchemeName)
                .setEndpoint(m_stunServer.address()),
            [&done](SystemError::ErrorCode sysErrorCode)
            {
                done.set_value(sysErrorCode);
            });
        ASSERT_EQ(SystemError::noError, done.get_future().get());
    }

    void thenConnectSucceeded()
    {
        ASSERT_EQ(SystemError::noError, m_connectResults.pop());
    }

    void thenConnectFailed()
    {
        ASSERT_NE(SystemError::noError, m_connectResults.pop());
    }

    void thenHttpTunnelIsOpened()
    {
        assertStunClientIsAbleToPerformRequest(&m_client);
    }

    void thenScheduledRequestIsServed()
    {
        auto response = m_responses.pop();
        ASSERT_EQ(SystemError::noError, std::get<0>(response));
        ASSERT_EQ(
            nx::network::stun::MessageClass::successResponse,
            std::get<1>(response).header.messageClass);
    }

    void thenConnectionIsEstablished()
    {
        assertStunClientIsAbleToPerformRequest(&m_client);
    }

    void forceHttpConnectionUpgradeTunnel()
    {
        m_tunnelFactoryBak = http::tunneling::detail::ClientFactory::instance().setCustomFunc(
            [this](const std::string& /*tag*/, const nx::utils::Url& url)
            {
                return std::make_unique<http::tunneling::detail::ConnectionUpgradeTunnelClient>(
                    url, nullptr);
            });
    }

private:
    stun::AsyncClientWithHttpTunneling m_client;
    nx::utils::SyncQueue<std::tuple<SystemError::ErrorCode, nx::network::stun::Message>> m_responses;
    nx::network::stun::SocketServer m_stunServer;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_connectResults;
    std::unique_ptr<http::TestHttpServer> m_httpServer;
    std::optional<http::tunneling::detail::ClientFactory::Function> m_tunnelFactoryBak;

    nx::utils::Url httpUrl() const
    {
        if (m_httpServer)
        {
            return url::Builder().setScheme(http::kUrlSchemeName)
                .setEndpoint(m_httpServer->serverAddress());
        }

        return tunnelUrl();
    }

    void onResponseReceived(
        SystemError::ErrorCode sysErrorCode,
        nx::network::stun::Message response)
    {
        m_responses.push(std::make_tuple(sysErrorCode, std::move(response)));
    }
};

TEST_F(AsyncClientWithHttpTunneling, establishes_http_tunnel)
{
    whenConnectToStunOverHttpServer();
    thenHttpTunnelIsOpened();
}

TEST_F(AsyncClientWithHttpTunneling, scheduled_stun_request_is_sent_after_tunnel_established)
{
    givenScheduledStunRequest();
    whenConnectToStunOverHttpServer();
    thenScheduledRequestIsServed();
}

TEST_F(AsyncClientWithHttpTunneling, regular_stun_connection)
{
    whenConnectToRegularStunServer();
    thenConnectionIsEstablished();
}

TEST_F(AsyncClientWithHttpTunneling, http_ok_response_to_upgrade_results_connect_error)
{
    forceHttpConnectionUpgradeTunnel();

    givenHttpServerThatAlwaysResponds(http::StatusCode::ok);
    whenConnectToHttpServer();
    thenConnectFailed();
}

//-------------------------------------------------------------------------------------------------

namespace {

struct StunAsyncClientWithHttpTunnelingVsHttpServerTestTypes
{
    using ClientType = stun::AsyncClientWithHttpTunneling;
    using ServerType = StunOverHttpServer;
};

struct StunAsyncClientWithHttpTunnelingVsStunServerTestTypes
{
    using ClientType = stun::AsyncClientWithHttpTunneling;
    using ServerType = SimpleServer;
};

} // namespace

INSTANTIATE_TYPED_TEST_CASE_P(
    StunAsyncClientWithHttpTunnelingVsHttpServer,
    StunAsyncClientAcceptanceTest,
    StunAsyncClientWithHttpTunnelingVsHttpServerTestTypes);

INSTANTIATE_TYPED_TEST_CASE_P(
    StunAsyncClientWithHttpTunnelingVsStunServer,
    StunAsyncClientAcceptanceTest,
    StunAsyncClientWithHttpTunnelingVsStunServerTestTypes);

} // namespace test
} // namespace stun
} // namespace network
} // namespace nx
