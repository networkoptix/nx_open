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
#include "tunnel_connector_stub.h"
#include "tunnel_connection_stub.h"

namespace nx {
namespace network {
namespace cloud {
namespace test {

class CrossNatConnector:
    public TunnelConnector
{
public:
    ~CrossNatConnector()
    {
        if (m_connectorFactoryBak)
            ConnectorFactory::instance().setCustomFunc(std::move(m_connectorFactoryBak));
    }

protected:
    void givenEstablishedTunnelConnection()
    {
        whenEstablishConnection();
        thenConnectionIsEstablished();
    }

    void whenEstablishConnection()
    {
        auto connectResult = doSimpleConnectTest(
            std::chrono::milliseconds::zero(),
            nx::hpm::MediaServerEmulator::ActionToTake::proceedWithConnection,
            mediator().stunEndpoint());
        m_tunnelConnection = std::move(connectResult.connection);
    }

    void thenConnectionIsEstablished()
    {
        ASSERT_NE(nullptr, m_tunnelConnection.get());
    }

    void assertConnectionHasNotBeenStarted()
    {
        auto connectionStub = m_establishedTestConnections.pop();
        ASSERT_FALSE(connectionStub->isStarted());
    }

    void enableConnectorStub()
    {
        using namespace std::placeholders;

        m_connectorFactoryBak = ConnectorFactory::instance().setCustomFunc(
            std::bind(&CrossNatConnector::connectorFactory, this, _1, _2, _3, _4));
    }

private:
    std::unique_ptr<nx::network::cloud::AbstractOutgoingTunnelConnection> m_tunnelConnection;
    ConnectorFactory::Function m_connectorFactoryBak;
    nx::utils::SyncQueue<TunnelConnectionStub*> m_establishedTestConnections;

    virtual void SetUp() override
    {
        ASSERT_TRUE(mediator().startAndWaitUntilStarted());
    }

    CloudConnectors connectorFactory(
        const AddressEntry& targetAddress,
        const nx::String& /*connectSessionId*/,
        const hpm::api::ConnectResponse& /*response*/,
        std::unique_ptr<UDPSocket> /*udpSocket*/)
    {
        TunnelConnectorContext connectorContext;
        auto connector = std::make_unique<TunnelConnectorStub>(targetAddress);
        connector->setBehavior(TunnelConnectorStub::Behavior::reportSuccess);
        connector->setConnectionQueue(&m_establishedTestConnections);
        connectorContext.connector = std::move(connector);

        CloudConnectors cloudConnectors;
        cloudConnectors.push_back(std::move(connectorContext));

        return cloudConnectors;
    }
};

TEST_F(CrossNatConnector, timeout)
{
    const std::chrono::milliseconds connectTimeout(nx::utils::random::number(1000, 4000));

    // Timing out mediator response by providing incorrect mediator address to connector.
    const auto connectResult = doSimpleConnectTest(
        connectTimeout,
        nx::hpm::MediaServerEmulator::ActionToTake::ignoreIndication,
        nx::network::SocketAddress(nx::network::HostAddress::localhost, 10345));

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

TEST_F(CrossNatConnector, provides_not_started_connections)
{
    enableConnectorStub();

    givenEstablishedTunnelConnection();
    assertConnectionHasNotBeenStarted();
}

//-------------------------------------------------------------------------------------------------
// CrossNatConnectorNoNatTraversalMethodAvailable

class CrossNatConnectorNoNatTraversalMethodAvailable:
    public CrossNatConnector
{
public:
    CrossNatConnectorNoNatTraversalMethodAvailable():
        m_cloudConnectMaskBak(ConnectorFactory::getEnabledCloudConnectMask())
    {
        ConnectorFactory::setEnabledCloudConnectMask(0);
    }

    ~CrossNatConnectorNoNatTraversalMethodAvailable()
    {
        ConnectorFactory::setEnabledCloudConnectMask(m_cloudConnectMaskBak);
    }

private:
    int m_cloudConnectMaskBak;
};

TEST_F(CrossNatConnectorNoNatTraversalMethodAvailable, expecting_error)
{
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
    public TunnelConnector
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
        ASSERT_TRUE(m_redirector.server->bind(nx::network::SocketAddress(nx::network::HostAddress::localhost, 0)));
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
        nx::network::SocketAddress targetAddress)
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
