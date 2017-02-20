#include <gtest/gtest.h>

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
    test::TransactionConnectionHelper connectionHelper;
};

TEST_F(Ec2MserverCloudSynchronizationConnection, connection_drop_after_system_removal)
{
    constexpr static const auto kWaitTimeout = std::chrono::seconds(10);
    // TODO: #ak static data of ConnectionLockGuard prohibits from testing with more than one connection
    constexpr static const auto kConnectionsToCreateCount = 1;

    ASSERT_TRUE(startAndWaitUntilStarted());

    const auto account = addActivatedAccount2();
    const auto system = addRandomSystemToAccount(account);

    std::vector<int> connectionIds;
    for (int i = 0; i < kConnectionsToCreateCount; ++i)
    {
        connectionIds.push_back(
            connectionHelper.establishTransactionConnection(
                utils::UrlBuilder().setScheme("http")
                    .setHost(endpoint().address.toString()).setPort(endpoint().port),
                system.id,
                system.authKey));
    }

    for (const auto& connectionId: connectionIds)
    {
        ASSERT_TRUE(
            connectionHelper.waitForState(
                {::ec2::QnTransactionTransportBase::Connected,
                    ::ec2::QnTransactionTransportBase::ReadyForStreaming},
                connectionId,
                kWaitTimeout));
    }

    ASSERT_EQ(
        api::ResultCode::ok,
        unbindSystem(account.email, account.password, system.id));

    for (const auto& connectionId: connectionIds)
    {
        ASSERT_TRUE(
            connectionHelper.waitForState(
                {::ec2::QnTransactionTransportBase::Closed,
                 ::ec2::QnTransactionTransportBase::Error},
                connectionId,
                kWaitTimeout));
    }
}

TEST_F(Ec2MserverCloudSynchronizationConnection, multiple_connections)
{
    constexpr const int maxAllowedConcurrentConnections = 2;
    constexpr const int maxConcurrentConnectionsToCreate = 5;
    static_assert(
        maxConcurrentConnectionsToCreate > maxAllowedConcurrentConnections,
        "Check values");
    constexpr const auto delayBeforeCheckingConnectionState = std::chrono::seconds(3);

    ASSERT_TRUE(startAndWaitUntilStarted());

    const auto account = addActivatedAccount2();
    const auto system = addRandomSystemToAccount(account);

    std::vector<int> connectionIds;
    for (int i = 0; i < maxConcurrentConnectionsToCreate; ++i)
        connectionIds.push_back(
            connectionHelper.establishTransactionConnection(
                utils::UrlBuilder().setScheme("http")
                    .setHost(endpoint().address.toString()).setPort(endpoint().port),
                system.id,
                system.authKey));

    std::this_thread::sleep_for(delayBeforeCheckingConnectionState);

    // Checking that exactly maxAllowedConcurrentConnections have succeeded.
    int activeConnections = 0;
    for (const auto& connectionId: connectionIds)
    {
        if (connectionHelper.isConnectionActive(connectionId))
            ++activeConnections;
    }

    ASSERT_EQ(maxAllowedConcurrentConnections, activeConnections);
}

TEST_F(Ec2MserverCloudSynchronizationConnection, checking_connection_blink_stability)
{
    constexpr int maxConcurrentConnectionsToCreate = 50;
    constexpr auto testRunTime = std::chrono::seconds(10);

    ASSERT_TRUE(startAndWaitUntilStarted());

    const auto account = addActivatedAccount2();
    const auto system = addRandomSystemToAccount(account);

    const auto runUntil = std::chrono::steady_clock::now() + testRunTime;
    while (std::chrono::steady_clock::now() < runUntil)
    {
        std::vector<int> connectionIds;
        for (int i = 0; i < maxConcurrentConnectionsToCreate; ++i)
            connectionIds.push_back(
                connectionHelper.establishTransactionConnection(
                    utils::UrlBuilder().setScheme("http")
                        .setHost(endpoint().address.toString()).setPort(endpoint().port),
                    system.id,
                    system.authKey));

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        connectionHelper.closeAllConnections();
    }
}

} // namespace cdb
} // namespace nx
