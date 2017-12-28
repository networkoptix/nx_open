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

#include "stun_async_client_acceptance_tests.h"

namespace nx {
namespace stun {
namespace test {

class StunOverHttpServer:
    public AbstractStunServer
{
public:
    StunOverHttpServer();

    virtual bool bind(const SocketAddress& localEndpoint) override;
    virtual bool listen() override;
    virtual utils::Url getServerUrl() const override;
    virtual nx::network::stun::MessageDispatcher& dispatcher() override;
    virtual void sendIndicationThroughEveryConnection(nx::network::stun::Message) override;
    virtual std::size_t connectionCount() const override;

private:
    TestHttpServer m_httpServer;
    nx::network::stun::MessageDispatcher m_dispatcher;
    nx::network::stun::StunOverHttpServer m_stunOverHttpServer;
};

//-------------------------------------------------------------------------------------------------

class StunOverHttpServerFixture:
    public ::testing::Test
{
protected:
    virtual void SetUp() override;

    nx::network::stun::MessageDispatcher& dispatcher();
    utils::Url tunnelUrl() const;
    nx::network::stun::Message popReceivedMessage();
    nx::network::stun::Message prepareRequest();

    void givenTunnelingServer();
    void assertStunClientIsAbleToPerformRequest(AbstractAsyncClient* client);

private:
    StunOverHttpServer m_server;
    nx::utils::SyncQueue<nx::network::stun::Message> m_messagesReceived;

    void processStunMessage(
        std::shared_ptr<nx::network::stun::AbstractServerConnection> serverConnection,
        nx::network::stun::Message message);
};

} // namespace test
} // namespace stun
} // namespace nx
