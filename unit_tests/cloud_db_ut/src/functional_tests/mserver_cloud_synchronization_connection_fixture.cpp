#include "mserver_cloud_synchronization_connection_fixture.h"

#include <nx/utils/test_support/utils.h>
#include <nx/utils/url_builder.h>

#include <nx_ec/ec_proto_version.h>

namespace nx {
namespace cdb {
namespace test {

Ec2MserverCloudSynchronizationConnection::Ec2MserverCloudSynchronizationConnection()
{
    NX_GTEST_ASSERT_TRUE(startAndWaitUntilStarted());

    m_account = addActivatedAccount2();
    m_system = addRandomSystemToAccount(m_account);
}

const AccountWithPassword& Ec2MserverCloudSynchronizationConnection::account() const
{
    return m_account;
}

const api::SystemData& Ec2MserverCloudSynchronizationConnection::system() const
{
    return m_system;
}

void Ec2MserverCloudSynchronizationConnection::openTransactionConnections(int count)
{
    openTransactionConnectionsOfSpecifiedVersion(count, nx_ec::EC2_PROTO_VERSION);
}

void Ec2MserverCloudSynchronizationConnection::openTransactionConnectionsOfSpecifiedVersion(
    int count, int protoVersion)
{
    for (int i = 0; i < count; ++i)
    {
        m_connectionIds.push_back(
            m_connectionHelper.establishTransactionConnection(
                utils::UrlBuilder().setScheme("http")
                .setHost(endpoint().address.toString()).setPort(endpoint().port),
                system().id,
                system().authKey,
                KeepAlivePolicy::enableKeepAlive,
                protoVersion));
    }
}

void Ec2MserverCloudSynchronizationConnection::waitForConnectionsToMoveToACertainState(
    const std::vector<::ec2::QnTransactionTransportBase::State>& desiredStates,
    std::chrono::milliseconds waitTimeout)
{
    for (const auto& connectionId : m_connectionIds)
        ASSERT_TRUE(m_connectionHelper.waitForState(desiredStates, connectionId, waitTimeout));
}

int Ec2MserverCloudSynchronizationConnection::activeConnectionCount() const
{
    int activeConnections = 0;
    for (const auto& connectionId : m_connectionIds)
    {
        if (m_connectionHelper.isConnectionActive(connectionId))
            ++activeConnections;
    }

    return activeConnections;
}

void Ec2MserverCloudSynchronizationConnection::waitUntilActiveConnectionCountReaches(int count)
{
    waitForAtLeastNConnectionsToMoveToACertainState(
        count,
        { ::ec2::QnTransactionTransportBase::Connected,
        ::ec2::QnTransactionTransportBase::ReadyForStreaming });
}

void Ec2MserverCloudSynchronizationConnection::waitUntilClosedConnectionCountReaches(int count)
{
    waitForAtLeastNConnectionsToMoveToACertainState(
        count,
        { ::ec2::QnTransactionTransportBase::Closed,
        ::ec2::QnTransactionTransportBase::Error });
}

void Ec2MserverCloudSynchronizationConnection::closeAllConnections()
{
    m_connectionHelper.closeAllConnections();
}

void Ec2MserverCloudSynchronizationConnection::setOnConnectionBecomesActive(
    test::TransactionConnectionHelper::ConnectionStateChangeHandler handler)
{
    m_connectionHelper.setOnConnectionBecomesActive(std::move(handler));
}

void Ec2MserverCloudSynchronizationConnection::setOnConnectionFailure(
    test::TransactionConnectionHelper::ConnectionStateChangeHandler handler)
{
    m_connectionHelper.setOnConnectionFailure(std::move(handler));
}

void Ec2MserverCloudSynchronizationConnection::waitForAtLeastNConnectionsToMoveToACertainState(
    int count,
    const std::vector<::ec2::QnTransactionTransportBase::State>& desiredStates)
{
    constexpr auto kCheckStatePeriod = std::chrono::milliseconds(100);

    while (numberOfConnectionsInACertainState(desiredStates) < count)
        std::this_thread::sleep_for(kCheckStatePeriod);
}

int Ec2MserverCloudSynchronizationConnection::numberOfConnectionsInACertainState(
    const std::vector<::ec2::QnTransactionTransportBase::State>& desiredStates)
{
    int count = 0;
    for (const auto& connectionId : m_connectionIds)
    {
        if (std::count(desiredStates.begin(), desiredStates.end(),
            m_connectionHelper.getConnectionStateById(connectionId)) > 0)
        {
            ++count;
        }
    }

    return count;
}

} // namespace test 
} // namespace cdb
} // namespace nx
