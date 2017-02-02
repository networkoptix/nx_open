#include <gtest/gtest.h>

#include <memory>

#include <nx/network/cloud/tunnel/cross_nat_connector.h>
#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/stun/udp_server.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>

#include <libconnection_mediator/src/test_support/mediator_functional_test.h>

#include "cross_nat_connector_test.h"

namespace nx {
namespace network {
namespace cloud {
namespace test {

class CrossNatConnector:
    public TunnelConnector
{
public:
    CrossNatConnector()
    {
        init();
    }

private:
    void init()
    {
        ASSERT_TRUE(mediator().startAndWaitUntilStarted());
    }
};

TEST_F(CrossNatConnector, timeout)
{
    const std::chrono::milliseconds connectTimeout(nx::utils::random::number(1000, 4000));

    // Timing out mediator response by providing incorrect mediator address to connector.
    const auto connectResult = doSimpleConnectTest(
        connectTimeout,
        nx::hpm::MediaServerEmulator::ActionToTake::ignoreIndication,
        SocketAddress(HostAddress::localhost, 10345));

    ASSERT_EQ(SystemError::timedOut, connectResult.errorCode);
    ASSERT_EQ(nullptr, connectResult.connection);
    //ASSERT_TRUE(
    //    connectResult.executionTime > connectTimeout*0.8 &&
    //    connectResult.executionTime < connectTimeout*1.2);
}

TEST_F(CrossNatConnector, target_host_not_found)
{
    const auto system1 = mediator().addRandomSystem();
    const auto server1 = mediator().addRandomServer(system1);

    auto incorrectSystem = system1;
    incorrectSystem.id += "_hren";

    const auto connectResult = doSimpleConnectTest(
        std::chrono::seconds::zero(),   //no timeout
        nx::hpm::MediaServerEmulator::ActionToTake::proceedWithConnection,
        incorrectSystem,
        server1);

    ASSERT_EQ(SystemError::hostNotFound, connectResult.errorCode);
    ASSERT_EQ(nullptr, connectResult.connection);
}

TEST_F(CrossNatConnector, no_nat_traversal_method_available)
{
    ConnectorFactory::setEnabledCloudConnectMask(0);

    const auto system1 = mediator().addRandomSystem();
    const auto server1 = mediator().addRandomServer(system1);

    const auto connectResult = doSimpleConnectTest(
        std::chrono::milliseconds::zero(),
        nx::hpm::MediaServerEmulator::ActionToTake::proceedWithConnection);

    ASSERT_NE(SystemError::noError, connectResult.errorCode);
}

//-------------------------------------------------------------------------------------------------
// CrossNatConnectorRedirect

class CrossNatConnectorRedirect:
    public CrossNatConnector
{
public:
    CrossNatConnectorRedirect():
        m_requestsRedirected(0)
    {
    }

    ~CrossNatConnectorRedirect()
    {
        if (m_connectResult.connection)
            m_connectResult.connection->pleaseStopSync();
    }

protected:
    void givenMediator()
    {
        ASSERT_TRUE(mediator().startAndWaitUntilStarted());
    }

    void givenRedirector()
    {
        using namespace std::placeholders;

        m_redirector.server = std::make_unique<stun::UdpServer>(&m_redirector.messageDispatcher);
        ASSERT_TRUE(m_redirector.server->bind(SocketAddress(HostAddress::localhost, 0)));
        ASSERT_TRUE(m_redirector.server->listen());

        m_redirector.messageDispatcher.registerRequestProcessor(
            stun::extension::methods::connect,
            std::bind(&CrossNatConnectorRedirect::redirectHandler, this,
                _1, _2, mediator().stunEndpoint()));
    }

    void whenRequestingConnectSessionFromRedirectingMediator()
    {
        m_connectResult = doSimpleConnectTest(
            std::chrono::seconds(3),
            nx::hpm::MediaServerEmulator::ActionToTake::proceedWithConnection,
            m_redirector.server->address());
    }

    void thenRealConnectShouldGoThroughFunctioningMediator()
    {
        ASSERT_GT(m_requestsRedirected.load(), 0);

        ASSERT_EQ(SystemError::noError, m_connectResult.errorCode);
        ASSERT_NE(nullptr, m_connectResult.connection);
    }

private:
    struct ServerContext
    {
        stun::MessageDispatcher messageDispatcher;
        std::unique_ptr<stun::UdpServer> server;
    };

    ServerContext m_redirector;
    std::unique_ptr<cloud::CrossNatConnector> m_crossNatConnector;
    std::atomic<int> m_requestsRedirected;
    TunnelConnector::ConnectResult m_connectResult;

    void redirectHandler(
        std::shared_ptr<stun::AbstractServerConnection> connection,
        stun::Message message,
        SocketAddress targetAddress)
    {
        ++m_requestsRedirected;

        stun::Message response(stun::Header(
            stun::MessageClass::errorResponse, 
            message.header.method,
            message.header.transactionId));
        response.newAttribute<stun::attrs::ErrorCode>(stun::error::tryAlternate);
        response.newAttribute<stun::attrs::AlternateServer>(targetAddress);
        connection->sendMessage(std::move(response), nullptr);
    }
};

TEST_F(CrossNatConnectorRedirect, redirect_to_alternate_mediator_instance)
{
    givenMediator();
    givenRedirector();

    whenRequestingConnectSessionFromRedirectingMediator();

    thenRealConnectShouldGoThroughFunctioningMediator();
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
