#include <gtest/gtest.h>

#include <transaction/connection_guard_shared_state.h>

#include "ec2/cloud_vms_synchro_test_helper.h"
#include "transaction_transport.h"

namespace nx {
namespace cdb {

/**
 * This test launches cdb and establishes transaction connection(s) to it.
 * Does not bring up whole appserver2 peer.
 */
class Ec2MserverCloudSynchronizationConnection
:
    public CdbFunctionalTest
{
public:
    Ec2MserverCloudSynchronizationConnection()
    :
        m_moduleGuid(QnUuid::createUuid()),
        m_runningInstanceGuid(QnUuid::createUuid()),
        m_transactionConnectionIdSequence(0)
    {
    }

    /**
     * @return new connection id
     */
    int openTransactionConnection(
        const std::string& systemId,
        const std::string& systemAuthKey)
    {
        auto localPeerInfo = localPeer();
        localPeerInfo.id = QnUuid::createUuid();

        ConnectionContext connectionContext;
        connectionContext.connectionGuardSharedState = 
            std::make_unique<::ec2::ConnectionGuardSharedState>();
        connectionContext.connection =
            std::make_unique<test::TransactionTransport>(
                connectionContext.connectionGuardSharedState.get(),
                localPeerInfo,
                systemId,
                systemAuthKey);
        QObject::connect(
            connectionContext.connection.get(), &test::TransactionTransport::stateChanged,
            connectionContext.connection.get(),
            [transactionConnection = connectionContext.connection.get(), this](
                test::TransactionTransport::State state)
            {
                onTransactionConnectionStateChanged(transactionConnection, state);
            },
            Qt::DirectConnection);

        std::lock_guard<std::mutex> lk(m_mutex);
        auto transactionConnectionPtr = connectionContext.connection.get();
        const int connectionId = ++m_transactionConnectionIdSequence;
        m_connections.emplace(
            connectionId,
            std::move(connectionContext));
        const QUrl url(lit("http://%1/ec2/events").arg(endpoint().toString()));
        transactionConnectionPtr->doOutgoingConnect(url);

        return connectionId;
    }
    
    bool waitForState(
        const std::vector<::ec2::QnTransactionTransportBase::State> desiredStates,
        int connectionId,
        std::chrono::milliseconds durationToWait)
    {
        std::unique_lock<std::mutex> lk(m_mutex);

        const auto waitUntil = std::chrono::steady_clock::now() + durationToWait;
        boost::optional<::ec2::QnTransactionTransportBase::State> prevState;
        for (;;)
        {
            auto connectionIter = m_connections.find(connectionId);
            if (connectionIter == m_connections.end())
            {
                // Connection has been removed.
                for (const auto& desiredState: desiredStates)
                    if (desiredState == test::TransactionTransport::Closed)
                        return true;
                return false;
            }

            const auto currentState = connectionIter->second.connection->getState();

            for (const auto& desiredState : desiredStates)
                if (currentState == desiredState)
                    return true;

            if (m_condition.wait_until(lk, waitUntil) == std::cv_status::timeout)
                return false;
        }
    }

    ::ec2::QnTransactionTransportBase::State getConnectionStateById(
        int connectionId) const
    {
        std::unique_lock<std::mutex> lk(m_mutex);

        auto connectionIter = m_connections.find(connectionId);
        if (connectionIter == m_connections.end())
            return ::ec2::QnTransactionTransportBase::NotDefined;

        return connectionIter->second.connection->getState();
    }

    bool isConnectionActive(int connectionId) const
    {
        const auto state = getConnectionStateById(connectionId);
        return state == ::ec2::QnTransactionTransportBase::Connected
            || state == ::ec2::QnTransactionTransportBase::NeedStartStreaming
            || state == ::ec2::QnTransactionTransportBase::ReadyForStreaming;
    }

private:
    struct ConnectionContext
    {
        /**
         * Keeping separate instance with each connection to allow 
         * multiple connections to same peer be created.
         */
        std::unique_ptr<::ec2::ConnectionGuardSharedState> connectionGuardSharedState;
        std::unique_ptr<test::TransactionTransport> connection;
    };

    QnUuid m_moduleGuid;
    QnUuid m_runningInstanceGuid;
    std::map<int, ConnectionContext> m_connections;
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    std::atomic<int> m_transactionConnectionIdSequence;

    ec2::ApiPeerData localPeer() const
    {
        return ec2::ApiPeerData(
            m_moduleGuid,
            m_runningInstanceGuid,
            Qn::PT_Server);
    }

    void onTransactionConnectionStateChanged(
        ec2::QnTransactionTransportBase* /*connection*/,
        ec2::QnTransactionTransportBase::State /*newState*/)
    {
        m_condition.notify_all();
    }
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
        connectionIds.push_back(
            openTransactionConnection(system.id, system.authKey));

    for (const auto& connectionId: connectionIds)
    {
        ASSERT_TRUE(
            waitForState(
                {::ec2::QnTransactionTransportBase::Connected},
                connectionId,
                kWaitTimeout));
    }

    ASSERT_EQ(
        api::ResultCode::ok,
        unbindSystem(account.data.email, account.password, system.id));

    for (const auto& connectionId: connectionIds)
    {
        ASSERT_TRUE(
            waitForState(
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
            openTransactionConnection(system.id, system.authKey));

    std::this_thread::sleep_for(delayBeforeCheckingConnectionState);

    // Checking that exactly maxAllowedConcurrentConnections have succeeded.
    int activeConnections = 0;
    for (const auto& connectionId: connectionIds)
    {
        if (isConnectionActive(connectionId))
            ++activeConnections;
    }

    ASSERT_EQ(maxAllowedConcurrentConnections, activeConnections);
}

} // namespace cdb
} // namespace nx
