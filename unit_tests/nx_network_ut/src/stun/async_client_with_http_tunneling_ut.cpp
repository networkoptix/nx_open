#include <tuple>

#include <gtest/gtest.h>

#include <nx/network/stun/async_client_with_http_tunneling.h>
#include <nx/network/stun/stream_socket_server.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/std/future.h>

#include "stun_over_http_server_fixture.h"

namespace nx {
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
        using namespace std::placeholders;

        m_client.sendRequest(
            prepareRequest(),
            std::bind(&AsyncClientWithHttpTunneling::onResponseReceived, this, _1, _2));
    }

    void whenConnectToStunOverHttpServer()
    {
        nx::utils::promise<SystemError::ErrorCode> connected;

        m_client.connect(
            tunnelUrl(),
            [&connected](SystemError::ErrorCode sysErrorCode)
            {
                connected.set_value(sysErrorCode);
            });
        ASSERT_EQ(SystemError::noError, connected.get_future().get());
    }
    
    void whenConnectToRegularStunServer()
    {
        nx::utils::promise<SystemError::ErrorCode> done;
        m_client.connect(
            network::url::Builder().setScheme("stun").setEndpoint(m_stunServer.address()),
            [&done](SystemError::ErrorCode sysErrorCode)
            {
                done.set_value(sysErrorCode);
            });
        ASSERT_EQ(SystemError::noError, done.get_future().get());
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
            nx::stun::MessageClass::successResponse,
            std::get<1>(response).header.messageClass);
    }

    void thenConnectionIsEstablished()
    {
        assertStunClientIsAbleToPerformRequest(&m_client);
    }

private:
    stun::AsyncClientWithHttpTunneling m_client;
    nx::utils::SyncQueue<std::tuple<SystemError::ErrorCode, nx::stun::Message>> m_responses;
    nx::stun::SocketServer m_stunServer;

    void onResponseReceived(
        SystemError::ErrorCode sysErrorCode,
        nx::stun::Message response)
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

} // namespace test
} // namespace stun
} // namespace nx
