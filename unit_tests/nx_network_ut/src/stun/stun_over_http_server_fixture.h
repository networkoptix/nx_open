#pragma once

#include <gtest/gtest.h>

#include <nx/network/http/http_client.h>
#include <nx/network/http/server/handler/http_server_handler_create_tunnel.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/stun/abstract_async_client.h>
#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/stun/server_connection.h>
#include <nx/network/stun/stun_over_http_server.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace stun {
namespace test {

class StunOverHttpServer
{
public:
    StunOverHttpServer();

    bool bind(const SocketAddress& localEndpoint);
    bool listen();
    QUrl getServerUrl() const;

    nx::stun::MessageDispatcher& dispatcher();

private:
    TestHttpServer m_httpServer;
    nx::stun::MessageDispatcher m_dispatcher;
    nx::stun::StunOverHttpServer m_stunOverHttpServer;
};

//-------------------------------------------------------------------------------------------------

class StunOverHttpServerFixture:
    public ::testing::Test
{
protected:
    virtual void SetUp() override;

    nx::stun::MessageDispatcher& dispatcher();
    QUrl tunnelUrl() const;
    nx::stun::Message popReceivedMessage();
    nx::stun::Message prepareRequest();

    void givenTunnelingServer();
    void assertStunClientIsAbleToPerformRequest(AbstractAsyncClient* client);

private:
    StunOverHttpServer m_server;
    nx::utils::SyncQueue<nx::stun::Message> m_messagesReceived;

    void processStunMessage(
        std::shared_ptr<nx::stun::AbstractServerConnection> serverConnection,
        nx::stun::Message message);
};

} // namespace test
} // namespace stun
} // namespace nx
