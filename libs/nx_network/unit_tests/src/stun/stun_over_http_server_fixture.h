// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gtest/gtest.h>

#include <nx/network/http/http_client.h>
#include <nx/network/http/server/handler/http_server_handler_create_tunnel.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/stun/abstract_async_client.h>
#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/stun/server_connection.h>
#include <nx/network/stun/stun_over_http_server.h>
#include <nx/network/test_support/stun_async_client_acceptance_tests.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace network {
namespace stun {
namespace test {

class StunOverHttpServer:
    public AbstractStunServer
{
public:
    StunOverHttpServer();

    virtual bool bind(const network::SocketAddress& localEndpoint) override;
    virtual bool listen() override;
    virtual nx::utils::Url url() const override;
    virtual nx::network::stun::MessageDispatcher& dispatcher() override;
    virtual void sendIndicationThroughEveryConnection(stun::Message) override;
    virtual std::size_t connectionCount() const override;

private:
    nx::network::stun::MessageDispatcher m_dispatcher;
    nx::network::stun::StunOverHttpServer m_stunOverHttpServer;
    network::http::TestHttpServer m_httpServer;
};

//-------------------------------------------------------------------------------------------------

class StunOverHttpServerFixture:
    public ::testing::Test
{
protected:
    virtual void SetUp() override;

    nx::network::stun::MessageDispatcher& dispatcher();
    nx::utils::Url tunnelUrl() const;
    nx::network::stun::Message popReceivedMessage();
    nx::network::stun::Message prepareRequest();

    void givenTunnelingServer();
    void assertStunClientIsAbleToPerformRequest(AbstractAsyncClient* client);

private:
    StunOverHttpServer m_server;
    nx::utils::SyncQueue<nx::network::stun::Message> m_messagesReceived;

    void processStunMessage(nx::network::stun::MessageContext ctx);
};

} // namespace test
} // namespace stun
} // namespace network
} // namespace nx
