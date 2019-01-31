#include <gtest/gtest.h>

#include <nx/utils/random.h>

#include "mserver_cloud_synchronization_connection_fixture.h"

namespace nx::cloud::db {
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

TEST_F(Ec2MserverCloudSynchronizationConnection, cloud_db_is_stable_when_connections_blink)
{
    constexpr int maxConcurrentConnectionsToCreate = 50;
    constexpr auto iterationCount = 2;

    for (int i = 0; i < iterationCount; ++i)
    {
        openTransactionConnections(maxConcurrentConnectionsToCreate);
        std::this_thread::sleep_for(
            std::chrono::milliseconds(nx::utils::random::number<int>(100, 999)));
        closeAllConnections();
    }
}

} // namespace test
} // namespace nx::cloud::db
