#include <gtest/gtest.h>

#include <nx/utils/test_support/utils.h>
#include <nx/utils/url_builder.h>

#include <transaction/connection_guard_shared_state.h>
#include <test_support/transaction_connection_helper.h>

#include "ec2/cloud_vms_synchro_test_helper.h"

namespace nx {
namespace cdb {

/**
 * This test launches cdb and establishes transaction connection(s) to it.
 * Does not bring up whole appserver2 peer.
 */
class Ec2MserverCloudSynchronizationConnection:
    public CdbFunctionalTest
{
public:
    Ec2MserverCloudSynchronizationConnection()
    {
        NX_GTEST_ASSERT_TRUE(startAndWaitUntilStarted());

        m_account = addActivatedAccount2();
        m_system = addRandomSystemToAccount(m_account);
    }

protected:
    const AccountWithPassword& account() const
    {
        return m_account;
    }

    const api::SystemData& system() const
    {
        return m_system;
    }

    void openNumberOfTransactionConnections(int count)
    {
        for (int i = 0; i < count; ++i)
        {
            m_connectionIds.push_back(
                m_connectionHelper.establishTransactionConnection(
                    utils::UrlBuilder().setScheme("http")
                    .setHost(endpoint().address.toString()).setPort(endpoint().port),
                    system().id,
                    system().authKey));
        }
    }

    void waitForConnectionsToMoveToACertainState(
        const std::vector<::ec2::QnTransactionTransportBase::State>& desiredStates,
        std::chrono::milliseconds waitTimeout)
    {
        for (const auto& connectionId: m_connectionIds)
            ASSERT_TRUE(m_connectionHelper.waitForState(desiredStates, connectionId, waitTimeout));
    }

    int activeConnectionCount() const
    {
        int activeConnections = 0;
        for (const auto& connectionId: m_connectionIds)
        {
            if (m_connectionHelper.isConnectionActive(connectionId))
                ++activeConnections;
        }

        return activeConnections;
    }

    void waitUntilActiveConnectionCountReaches(int count)
    {
        waitForAtLeastNConnectionsToMoveToACertainState(
            count,
            {::ec2::QnTransactionTransportBase::Connected,
                ::ec2::QnTransactionTransportBase::ReadyForStreaming});
    }

    void waitUntilClosedConnectionCountReaches(int count)
    {
        waitForAtLeastNConnectionsToMoveToACertainState(
            count,
            {::ec2::QnTransactionTransportBase::Closed,
                ::ec2::QnTransactionTransportBase::Error});
    }

    void closeAllConnections()
    {
        m_connectionHelper.closeAllConnections();
    }

private:
    AccountWithPassword m_account;
    api::SystemData m_system;
    test::TransactionConnectionHelper m_connectionHelper;
    std::vector<int> m_connectionIds;

    void waitForAtLeastNConnectionsToMoveToACertainState(
        int count,
        const std::vector<::ec2::QnTransactionTransportBase::State>& desiredStates)
    {
        constexpr auto kCheckStatePeriod = std::chrono::milliseconds(100);

        while (numberOfConnectionsInACertainState(desiredStates) < count)
            std::this_thread::sleep_for(kCheckStatePeriod);
    }

    int numberOfConnectionsInACertainState(
        const std::vector<::ec2::QnTransactionTransportBase::State>& desiredStates)
    {
        int count = 0;
        for (const auto& connectionId: m_connectionIds)
        {
            if (std::count(desiredStates.begin(), desiredStates.end(),
                    m_connectionHelper.getConnectionStateById(connectionId)) > 0)
            {
                ++count;
            }
        }

        return count;
    }
};

TEST_F(Ec2MserverCloudSynchronizationConnection, connection_drop_after_system_removal)
{
    constexpr static const auto kWaitTimeout = std::chrono::seconds(10);
    constexpr static const auto kConnectionsToCreateCount = 1;

    openNumberOfTransactionConnections(kConnectionsToCreateCount);

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

    openNumberOfTransactionConnections(maxConcurrentConnectionsToCreate);

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
        openNumberOfTransactionConnections(maxConcurrentConnectionsToCreate);

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        closeAllConnections();
    }
}

} // namespace cdb
} // namespace nx
