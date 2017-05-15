#include <memory>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/cloud_tunnel_connector_executor.h>
#include <nx/network/cloud/tunnel/connector_factory.h>
#include <nx/utils/std/future.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

//-------------------------------------------------------------------------------------------------
// ConnectorExecutorMethodPrioritization

class ConnectorExecutorMethodPrioritization:
    public ::testing::Test
{
protected:
    void setEveryMethodDoesNotReturnResult()
    {
        // TODO
    }

    void givenMediatorResponseWithMethodsOfDifferentPriority()
    {
        //m_mediatorResponse.udpHolePunchingPriority.delay = std::chrono::milliseconds::zero();
        //m_mediatorResponse.tcpForwardedPortPriority.delay = std::chrono::milliseconds::zero();
        //m_mediatorResponse.trafficRelayPriority.delay = std::chrono::hours(1);
    }

    void givenMediatorResponseWithMultipleMethodsOfTheSamePriority()
    {
        //m_mediatorResponse.udpHolePunchingPriority.delay = std::chrono::milliseconds::zero();
        //m_mediatorResponse.tcpForwardedPortPriority.delay = std::chrono::milliseconds::zero();
        //m_mediatorResponse.trafficRelayPriority.delay = std::chrono::milliseconds::zero();
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

    void thenPrioritizedMethodHasBeenInvokedFirst()
    {
        m_done.get_future().wait();
        // TODO
    }

    void thenEveryMethodHasBeenInvoked()
    {
        m_done.get_future().wait();
        // TODO
    }

private:
    nx::hpm::api::ConnectResponse m_mediatorResponse;
    std::unique_ptr<cloud::ConnectorExecutor> m_connectorExecutor;
    nx::utils::promise<void> m_done;

    void onDone(
        nx::hpm::api::NatTraversalResultCode /*resultCode*/,
        SystemError::ErrorCode /*sysErrorCode*/,
        std::unique_ptr<AbstractOutgoingTunnelConnection> /*connection*/)
    {
        m_done.set_value();
        // TODO
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
    setEveryMethodDoesNotReturnResult();

    givenMediatorResponseWithMethodsOfDifferentPriority();
    whenInvokedConnector();
    thenEveryMethodHasBeenInvoked();
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
