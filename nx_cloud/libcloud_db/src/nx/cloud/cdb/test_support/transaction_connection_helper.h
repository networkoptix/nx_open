#pragma once

#include <nx/network/aio/timer.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/thread/mutex.h>

#include <transaction/transaction_transport_base.h>
#include <nx/utils/subscription.h>

#include "test_transaction_transport.h"

namespace nx {
namespace cdb {
namespace test {

enum class KeepAlivePolicy
{
    enableKeepAlive,
    noKeepAlive,
};

using OnConnectionBecomesActiveSubscription =
    utils::Subscription<int /*connectionId*/, ::ec2::QnTransactionTransportBase::State>;
using OnConnectionFailureSubscription =
    utils::Subscription<int /*connectionId*/, ::ec2::QnTransactionTransportBase::State>;

/**
 * Helps to establish transaction connection to appserver2 peer and monitor its state.
 */
class TransactionConnectionHelper
{
public:
    struct ConnectionContext
    {
        ::ec2::ApiPeerData peerInfo;
        /**
         * Keeping separate instance with each connection to allow
         * multiple connections to same peer be created.
         */
        std::unique_ptr<::ec2::ConnectionGuardSharedState> connectionGuardSharedState;
        std::unique_ptr<test::TransactionTransport> connection;
        std::unique_ptr<nx::network::aio::Timer> timer;
    };

    using ConnectionId = int;

    TransactionConnectionHelper();
    ~TransactionConnectionHelper();

    void setRemoveConnectionAfterClosure(bool val);
    void setMaxDelayBeforeConnect(std::chrono::milliseconds delay);

    /**
     * @return New connection id.
     */
    ConnectionId establishTransactionConnection(
        const nx::utils::Url& appserver2BaseUrl,
        const std::string& login,
        const std::string& password,
        KeepAlivePolicy keepAlivePolicy,
        int protocolVersion,
        const QnUuid& peerId = QnUuid());
    
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
        func(it == m_connections.end() ? nullptr : &it->second);
    }

    template<typename Func>
    void getAccessToConnectionByIndex(std::size_t index, Func func)
    {
        QnMutexLocker lk(&m_mutex);
        auto it = std::next(m_connections.begin(), index);
        func(it == m_connections.end() ? nullptr : &it->second);
    }

    void closeAllConnections();

    std::size_t activeConnectionCount() const;
    std::size_t totalFailedConnections() const;
    std::size_t connectedConnections() const;

    OnConnectionBecomesActiveSubscription& onConnectionBecomesActiveSubscription();
    OnConnectionFailureSubscription& onConnectionFailureSubscription();

private:
    QnUuid m_moduleGuid;
    QnUuid m_runningInstanceGuid;
    std::map<ConnectionId, ConnectionContext> m_connections;
    std::set<ConnectionId> m_connectedConnections;
    std::atomic<std::size_t> m_totalConnectionsFailed;
    mutable QnMutex m_mutex;
    QnWaitCondition m_condition;
    std::atomic<ConnectionId> m_transactionConnectionIdSequence;
    nx::network::aio::Timer m_aioTimer;
    bool m_removeConnectionAfterClosure;
    std::chrono::milliseconds m_maxDelayBeforeConnect;
    OnConnectionBecomesActiveSubscription m_onConnectionBecomesActiveSubscription;
    OnConnectionFailureSubscription m_onConnectionFailureSubscription;

    ConnectionContext prepareConnectionContext(
        const std::string& login,
        const std::string& password,
        KeepAlivePolicy keepAlivePolicy,
        int protocolVersion,
        const QnUuid& peerId);
    void startConnection(ConnectionContext* connectionContext,
        const utils::Url &appserver2BaseUrl);

    ::ec2::ApiPeerData localPeer() const;

    void onTransactionConnectionStateChanged(
        ::ec2::QnTransactionTransportBase* /*connection*/,
        ::ec2::QnTransactionTransportBase::State /*newState*/);

    nx::utils::Url prepareTargetUrl(const nx::utils::Url& appserver2BaseUrl, const QnUuid& localPeerId);

    void moveConnectionToReadyForStreamingState(
        ::ec2::QnTransactionTransportBase* connection);
    void removeConnection(::ec2::QnTransactionTransportBase* connection);
    ConnectionId getConnectionId(::ec2::QnTransactionTransportBase* connection) const;
};

} // namespace test
} // namespace cdb
} // namespace nx
