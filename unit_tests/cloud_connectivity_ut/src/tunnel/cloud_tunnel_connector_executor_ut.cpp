#include <memory>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/cloud_tunnel_connector_executor.h>
#include <nx/network/cloud/tunnel/connector_factory.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/utils/random.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/sync_queue.h>

#include "tunnel_connection_stub.h"
#include "tunnel_connector_stub.h"

namespace nx {
namespace network {
namespace cloud {
namespace test {

//-------------------------------------------------------------------------------------------------
// ConnectorExecutor

constexpr const int kPrioritizedMethodNumber = 1;
constexpr const int kNonPrioritizedMethodNumber = 2;

class ConnectorExecutor:
    public ::testing::Test
{
public:
    ~ConnectorExecutor()
    {
        stopConnectorExecutor();
    }

protected:
    void givenMediatorResponseWithEmptyUdpEndpointList()
    {
        m_mediatorResponse.forwardedTcpEndpointList.push_back(
            SocketAddress("127.0.0.1:12345"));
    }

    void givenEmptyMediatorResponse()
    {
    }

    void whenInvokeConnector()
    {
        using namespace std::placeholders;

        m_connectorExecutor = std::make_unique<cloud::ConnectorExecutor>(
            AddressEntry("Some_cloud_peer"),
            "connect_session_id",
            m_mediatorResponse,
            std::make_unique<UDPSocket>(AF_INET));
        m_connectorExecutor->start(
            std::bind(&ConnectorExecutor::onDone, this, _1, _2, _3));
    }

    void thenErrorIsReported()
    {
        m_prevConnectorResult = m_connectorResultQueue.pop();
        ASSERT_NE(
            nx::hpm::api::NatTraversalResultCode::ok,
            m_prevConnectorResult->resultCode);
    }

    void stopConnectorExecutor()
    {
        if (m_connectorExecutor)
            m_connectorExecutor->pleaseStopSync();
    }

private:
    struct Result
    {
        nx::hpm::api::NatTraversalResultCode resultCode;
        SystemError::ErrorCode sysErrorCode;
        std::unique_ptr<AbstractOutgoingTunnelConnection> connection;

        Result() = delete;
    };

    nx::hpm::api::ConnectResponse m_mediatorResponse;
    std::unique_ptr<cloud::ConnectorExecutor> m_connectorExecutor;
    nx::utils::SyncQueue<Result> m_connectorResultQueue;
    boost::optional<Result> m_prevConnectorResult;

    void onDone(
        nx::hpm::api::NatTraversalResultCode resultCode,
        SystemError::ErrorCode sysErrorCode,
        std::unique_ptr<AbstractOutgoingTunnelConnection> connection)
    {
        m_connectorResultQueue.push(
            {resultCode, sysErrorCode, std::move(connection)});
    }
};

TEST_F(ConnectorExecutor, empty_udp_endpoint_list_is_processed_properly)
{
    givenMediatorResponseWithEmptyUdpEndpointList();
    whenInvokeConnector();
    thenErrorIsReported();
}

TEST_F(ConnectorExecutor, empty_connect_response_is_processed_properly)
{
    givenEmptyMediatorResponse();
    whenInvokeConnector();
    thenErrorIsReported();
}

//-------------------------------------------------------------------------------------------------

class ConnectorExecutorMethodPrioritization:
    public ConnectorExecutor
{
public:
    ConnectorExecutorMethodPrioritization()
    {
        using namespace std::placeholders;

        m_connectorFactoryBak = ConnectorFactory::instance().setCustomFunc(
            std::bind(&ConnectorExecutorMethodPrioritization::connectorFactoryFunc, this,
                _1, _2, _3, _4));
    }

    ~ConnectorExecutorMethodPrioritization()
    {
        stopConnectorExecutor();

        ConnectorFactory::instance().setCustomFunc(
            std::move(m_connectorFactoryBak));
    }

protected:
    void givenMediatorResponseWithMethodsOfDifferentPriority()
    {
        for (int i = 0; i < 2; ++i)
        {
            m_connectorPriorities.push_back(
                { kNonPrioritizedMethodNumber, std::chrono::milliseconds(1) });
        }

        m_connectorPriorities.push_back(
            { kPrioritizedMethodNumber, std::chrono::milliseconds::zero() });
    }

    void givenMediatorResponseWithMultipleMethodsOfTheSamePriority()
    {
        for (int i = 0; i < 3; ++i)
        {
            m_connectorPriorities.push_back(
                { kPrioritizedMethodNumber, std::chrono::milliseconds::zero() });
        }
    }

    void whenAnyMethodHasSucceeded()
    {
        nx::utils::random::choice(m_createdTunnelConnectors)->setBehavior(
            TunnelConnectorStub::Behavior::reportSuccess);
    }

    void thenPrioritizedMethodHasBeenInvokedFirst()
    {
        ASSERT_EQ(kPrioritizedMethodNumber, m_connectorInvocation.pop());
    }

    void thenEveryMethodHasBeenInvoked()
    {
        for (std::size_t i = 0; i < m_connectorPriorities.size(); ++i)
            m_connectorInvocation.pop();
    }

    void thenAllMethodsAreDestroyed()
    {
        for (std::size_t i = 0; i < m_connectorPriorities.size(); ++i)
            m_destructionEventReceiver.pop();
    }

private:
    struct ConnectorPriority
    {
        int number;
        std::chrono::milliseconds startDelay;
    };

    ConnectorFactory::Function m_connectorFactoryBak;
    nx::utils::SyncQueue<int> m_connectorInvocation;
    nx::utils::SyncQueue<int> m_destructionEventReceiver;
    std::vector<TunnelConnectorStub*> m_createdTunnelConnectors;
    std::vector<ConnectorPriority> m_connectorPriorities;

    CloudConnectors connectorFactoryFunc(
        const AddressEntry& targetAddress,
        const nx::String& /*connectSessionId*/,
        const hpm::api::ConnectResponse& /*response*/,
        std::unique_ptr<UDPSocket> /*udpSocket*/)
    {
        CloudConnectors connectors;

        for (const auto& connectorPriority: m_connectorPriorities)
        {
            TunnelConnectorContext context;
            context.startDelay = connectorPriority.startDelay;
            auto dummyConnector = std::make_unique<TunnelConnectorStub>(
                targetAddress,
                connectorPriority.number,
                &m_connectorInvocation,
                &m_destructionEventReceiver);
            m_createdTunnelConnectors.push_back(dummyConnector.get());
            context.connector = std::move(dummyConnector);
            connectors.push_back(std::move(context));
        }

        return connectors;
    }
};

TEST_F(ConnectorExecutorMethodPrioritization, prioritized_method_invoked_first)
{
    givenMediatorResponseWithMethodsOfDifferentPriority();
    whenInvokeConnector();
    thenPrioritizedMethodHasBeenInvokedFirst();
}

TEST_F(ConnectorExecutorMethodPrioritization, multiple_methods_of_the_same_priority)
{
    givenMediatorResponseWithMultipleMethodsOfTheSamePriority();
    whenInvokeConnector();
    thenEveryMethodHasBeenInvoked();
}

TEST_F(ConnectorExecutorMethodPrioritization, less_prioritized_method_is_still_invoked)
{
    givenMediatorResponseWithMethodsOfDifferentPriority();
    whenInvokeConnector();
    thenEveryMethodHasBeenInvoked();
}

TEST_F(ConnectorExecutorMethodPrioritization, remaining_connectors_are_destroyed_on_success)
{
    givenMediatorResponseWithMethodsOfDifferentPriority();
    whenInvokeConnector();
    whenAnyMethodHasSucceeded();
    thenAllMethodsAreDestroyed();
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
