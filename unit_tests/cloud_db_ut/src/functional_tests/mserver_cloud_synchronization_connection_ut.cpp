#include <gtest/gtest.h>

#include "mserver_cloud_synchronization_connection_fixture.h"

namespace nx {
namespace cdb {
namespace test {

TEST_F(Ec2MserverCloudSynchronizationConnection, connection_drop_after_system_removal)
{
    constexpr static const auto kWaitTimeout = std::chrono::seconds(10);
    constexpr static const auto kConnectionsToCreateCount = 1;

    openTransactionConnections(kConnectionsToCreateCount);

    waitForConnectionsToMoveToACertainState(
        {::ec2::QnTransactionTransportBase::Connected,
            ::ec2::QnTransactionTransportBase::ReadyForStreaming},
        kWaitTimeout);

    ASSERT_EQ(
        api::ResultCode::ok,
        unbindSystem(account().email, account().password, system().id));

    waitForConnectionsToMoveToACertainState(
        {::ec2::QnTransactionTransportBase::Closed,
            ::ec2::QnTransactionTransportBase::Error},
        kWaitTimeout);
}

TEST_F(Ec2MserverCloudSynchronizationConnection, multiple_connections)
{
    constexpr const int maxAllowedConcurrentConnections = 2;
    constexpr const int maxConcurrentConnectionsToCreate = 5;
    static_assert(
        maxConcurrentConnectionsToCreate > maxAllowedConcurrentConnections,
        "Check values");

    openTransactionConnections(maxConcurrentConnectionsToCreate);

    // Waiting for active connection count to reach maxAllowedConcurrentConnections
    waitUntilActiveConnectionCountReaches(maxAllowedConcurrentConnections);

    // Waiting until every other connection has been rejected.
    waitUntilClosedConnectionCountReaches(
        maxConcurrentConnectionsToCreate - maxAllowedConcurrentConnections);

    // Checking active connection count once again.
    ASSERT_EQ(maxAllowedConcurrentConnections, activeConnectionCount());
}

TEST_F(Ec2MserverCloudSynchronizationConnection, checking_connection_blink_stability)
{
    constexpr int maxConcurrentConnectionsToCreate = 50;
    constexpr auto testRunTime = std::chrono::seconds(10);

    const auto runUntil = std::chrono::steady_clock::now() + testRunTime;
    while (std::chrono::steady_clock::now() < runUntil)
    {
        openTransactionConnections(maxConcurrentConnectionsToCreate);

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        closeAllConnections();
    }
}

} // namespace test
} // namespace cdb
} // namespace nx
