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

class StunOverHttpServerFixture:
    public ::testing::Test
{
public:
    StunOverHttpServerFixture();

protected:
    virtual void SetUp() override;

    QUrl tunnelUrl() const;
    nx::stun::Message popReceivedMessage();
    nx::stun::Message prepareRequest();

    void givenTunnelingServer();
    void assertStunClientIsAbleToPerformRequest(AbstractAsyncClient* client);

private:
    TestHttpServer m_httpServer;
    nx::utils::SyncQueue<nx::stun::Message> m_messagesReceived;
    nx::stun::MessageDispatcher m_dispatcher;
    nx::stun::StunOverHttpServer m_stunOverHttpServer;
    std::unique_ptr<AbstractStreamSocket> m_httpTunnelConnection;

    void processStunMessage(
        std::shared_ptr<nx::stun::AbstractServerConnection> serverConnection,
        nx::stun::Message message);
};

} // namespace test
} // namespace stun
} // namespace nx
