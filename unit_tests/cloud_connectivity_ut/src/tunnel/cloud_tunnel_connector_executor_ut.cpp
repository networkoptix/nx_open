#include <memory>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/cloud_tunnel_connector_executor.h>
#include <nx/network/cloud/tunnel/connector_factory.h>
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
// ConnectorExecutorMethodPrioritization

constexpr const int kPrioritizedMethodNumber = 1;
constexpr const int kNonPrioritizedMethodNumber = 2;

class ConnectorExecutorMethodPrioritization:
    public ::testing::Test
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
        if (m_connectorExecutor)
            m_connectorExecutor->pleaseStopSync();

        ConnectorFactory::instance().setCustomFunc(
            std::move(m_connectorFactoryBak));
    }

protected:
    void givenMediatorResponseWithMethodsOfDifferentPriority()
    {
        for (int i = 0; i < 2; ++i)
        {
            m_connectorPriorities.push_back(
                {kNonPrioritizedMethodNumber, std::chrono::milliseconds(1)});
        }

        m_connectorPriorities.push_back(
            {kPrioritizedMethodNumber, std::chrono::milliseconds::zero()});
    }

    void givenMediatorResponseWithMultipleMethodsOfTheSamePriority()
    {
        for (int i = 0; i < 3; ++i)
        {
            m_connectorPriorities.push_back(
                {kPrioritizedMethodNumber, std::chrono::milliseconds::zero()});
        }
    }

    void whenInvokedConnector()
    {
        using namespace std::placeholders;

        m_connectorExecutor = std::make_unique<cloud::ConnectorExecutor>(
            AddressEntry("Some_cloud_peer"),
            "connect_session_id",
            m_mediatorResponse,
            nullptr);
        m_connectorExecutor->start(
            std::bind(&ConnectorExecutorMethodPrioritization::onDone, this, _1, _2, _3));
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

    nx::hpm::api::ConnectResponse m_mediatorResponse;
    std::unique_ptr<cloud::ConnectorExecutor> m_connectorExecutor;
    nx::utils::promise<void> m_done;
    ConnectorFactory::Function m_connectorFactoryBak;
    std::vector<ConnectorPriority> m_connectorPriorities;
    nx::utils::SyncQueue<int> m_connectorInvocation;
    nx::utils::SyncQueue<int> m_destructionEventReceiver;
    std::vector<TunnelConnectorStub*> m_createdTunnelConnectors;

    void onDone(
        nx::hpm::api::NatTraversalResultCode /*resultCode*/,
        SystemError::ErrorCode /*sysErrorCode*/,
        std::unique_ptr<AbstractOutgoingTunnelConnection> /*connection*/)
    {
        m_done.set_value();
    }

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
    whenInvokedConnector();
    thenPrioritizedMethodHasBeenInvokedFirst();
}

TEST_F(ConnectorExecutorMethodPrioritization, multiple_methods_of_the_same_priority)
{
    givenMediatorResponseWithMultipleMethodsOfTheSamePriority();
    whenInvokedConnector();
    thenEveryMethodHasBeenInvoked();
}

TEST_F(ConnectorExecutorMethodPrioritization, less_prioritized_method_is_still_invoked)
{
    givenMediatorResponseWithMethodsOfDifferentPriority();
    whenInvokedConnector();
    thenEveryMethodHasBeenInvoked();
}

TEST_F(ConnectorExecutorMethodPrioritization, remaining_connectors_are_destroyed_on_success)
{
    givenMediatorResponseWithMethodsOfDifferentPriority();
    whenInvokedConnector();
    whenAnyMethodHasSucceeded();
    thenAllMethodsAreDestroyed();
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
