#pragma once

#include <nx/network/aio/timer.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/thread/mutex.h>

#include <transaction/transaction_transport_base.h>

#include "test_transaction_transport.h"

namespace nx {
namespace cdb {
namespace test {

enum class KeepAlivePolicy
{
    enableKeepAlive,
    noKeepAlive,
};

/**
 * Helps to establish transaction connection to appserver2 peer and monitor its state.
 */
class TransactionConnectionHelper
{
public:
    typedef int ConnectionId;

    TransactionConnectionHelper();
    ~TransactionConnectionHelper();

    void setRemoveConnectionAfterClosure(bool val);

    /**
     * @return New connection id.
     */
    ConnectionId establishTransactionConnection(
        const QUrl& appserver2BaseUrl,
        const std::string& login,
        const std::string& password,
        KeepAlivePolicy keepAlivePolicy = KeepAlivePolicy::enableKeepAlive);
    
    bool waitForState(
        const std::vector<::ec2::QnTransactionTransportBase::State> desiredStates,
        ConnectionId connectionId,
        std::chrono::milliseconds durationToWait);

    ::ec2::QnTransactionTransportBase::State getConnectionStateById(
        ConnectionId connectionId) const;

    bool isConnectionActive(ConnectionId connectionId) const;

    template<typename Func>
    void getAccessToConnectionById(ConnectionId connectionId, Func func)
    {
        QnMutexLocker lk(&m_mutex);
        auto it = m_connections.find(connectionId);
        func(it == m_connections.end() ? nullptr : it->second.connection.get());
    }

    void closeAllConnections();

    std::size_t activeConnectionCount() const;

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
    std::map<ConnectionId, ConnectionContext> m_connections;
    mutable QnMutex m_mutex;
    QnWaitCondition m_condition;
    std::atomic<ConnectionId> m_transactionConnectionIdSequence;
    nx::network::aio::Timer m_aioTimer;
    bool m_removeConnectionAfterClosure;

    ::ec2::ApiPeerData localPeer() const;

    void onTransactionConnectionStateChanged(
        ::ec2::QnTransactionTransportBase* /*connection*/,
        ::ec2::QnTransactionTransportBase::State /*newState*/);

    void moveConnectionToReadyForStreamingState(
        ec2::QnTransactionTransportBase* connection);
    void removeConnection(ec2::QnTransactionTransportBase* connection);
    ConnectionId getConnectionId(ec2::QnTransactionTransportBase* connection) const;
};

} // namespace test
} // namespace cdb
} // namespace nx
