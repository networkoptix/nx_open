#include "transaction_connection_helper.h"

#include <chrono>

#include <QUrlQuery>

#include <nx/utils/std/cpp14.h>
#include <cdb/ec2_request_paths.h>

namespace nx {
namespace cdb {
namespace test {

TransactionConnectionHelper::TransactionConnectionHelper():
    m_moduleGuid(QnUuid::createUuid()),
    m_runningInstanceGuid(QnUuid::createUuid()),
    m_totalConnectionsFailed(0),
    m_transactionConnectionIdSequence(0),
    m_removeConnectionAfterClosure(false)
{
}

TransactionConnectionHelper::~TransactionConnectionHelper()
{
    closeAllConnections();
    m_aioTimer.pleaseStopSync();
}

void TransactionConnectionHelper::setRemoveConnectionAfterClosure(bool val)
{
    m_removeConnectionAfterClosure = val;
}

TransactionConnectionHelper::ConnectionId
    TransactionConnectionHelper::establishTransactionConnection(
        const QUrl& appserver2BaseUrl,
        const std::string& login,
        const std::string& password,
        KeepAlivePolicy keepAlivePolicy,
        int protocolVersion,
        QnUuid peerId)
{
    auto localPeerInfo = localPeer();
    localPeerInfo.id = peerId.isNull() ? QnUuid::createUuid() : peerId;

    const auto str = localPeerInfo.id.toString();

    ConnectionContext connectionContext;
    connectionContext.peerInfo = localPeerInfo;
    connectionContext.connectionGuardSharedState =
        std::make_unique<::ec2::ConnectionGuardSharedState>();
    connectionContext.connection =
        std::make_unique<test::TransactionTransport>(
            connectionContext.connectionGuardSharedState.get(),
            localPeerInfo,
            login,
            password,
            protocolVersion,
            ++m_transactionConnectionIdSequence);
    connectionContext.connection->bindToAioThread(m_aioTimer.getAioThread());
    if (keepAlivePolicy == KeepAlivePolicy::noKeepAlive)
        connectionContext.connection->setKeepAliveEnabled(false);
    QObject::connect(
        connectionContext.connection.get(), &test::TransactionTransport::stateChanged,
        connectionContext.connection.get(),
        [transactionConnection = connectionContext.connection.get(), this](
            test::TransactionTransport::State state)
        {
            onTransactionConnectionStateChanged(transactionConnection, state);
        },
        Qt::DirectConnection);

    QnMutexLocker lk(&m_mutex);
    auto transactionConnectionPtr = connectionContext.connection.get();
    m_connections.emplace(
        transactionConnectionPtr->connectionId(),
        std::move(connectionContext));

    // TODO: #ak TransactionTransport should do that. But somehow it doesn't...
    QUrl url = appserver2BaseUrl;
    QUrlQuery urlQuery(url.query());
    urlQuery.addQueryItem("guid", localPeerInfo.id.toString());
    url.setQuery(urlQuery);
    url.setPath(api::kEc2EventsPath);
    transactionConnectionPtr->doOutgoingConnect(url);

    return transactionConnectionPtr->connectionId();
}

bool TransactionConnectionHelper::waitForState(
    const std::vector<::ec2::QnTransactionTransportBase::State> desiredStates,
    ConnectionId connectionId,
    std::chrono::milliseconds durationToWait)
{
    // TODO: #ak This method can skip Connected state due Connected -> ReadyForStreaming
    //    transition in onTransactionConnectionStateChanged.

    using namespace std::chrono;

    QnMutexLocker lk(&m_mutex);

    const auto waitUntil = steady_clock::now() + durationToWait;
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

        for (const auto& desiredState: desiredStates)
            if (currentState == desiredState)
                return true;

        const auto currentTime = steady_clock::now();
        if (waitUntil <= currentTime)
            return false;

        if (!m_condition.wait(
                lk.mutex(),
                duration_cast<milliseconds>(waitUntil - currentTime).count()))
        {
            return false;
        }
    }
}

::ec2::QnTransactionTransportBase::State
    TransactionConnectionHelper::getConnectionStateById(
        ConnectionId connectionId) const
{
    QnMutexLocker lk(&m_mutex);

    auto connectionIter = m_connections.find(connectionId);
    if (connectionIter == m_connections.end())
        return ::ec2::QnTransactionTransportBase::NotDefined;

    return connectionIter->second.connection->getState();
}

bool TransactionConnectionHelper::isConnectionActive(
    ConnectionId connectionId) const
{
    const auto state = getConnectionStateById(connectionId);
    return state == ::ec2::QnTransactionTransportBase::Connected
        || state == ::ec2::QnTransactionTransportBase::NeedStartStreaming
        || state == ::ec2::QnTransactionTransportBase::ReadyForStreaming;
}

void TransactionConnectionHelper::closeAllConnections()
{
    QnMutexLocker lk(&m_mutex);
    auto connections = std::move(m_connections);
    m_connections.clear();
    lk.unlock();

    for (auto& connectionContext: connections)
        connectionContext.second.connection->pleaseStopSync();
}

std::size_t TransactionConnectionHelper::activeConnectionCount() const
{
    QnMutexLocker lk(&m_mutex);
    return m_connections.size();
}

std::size_t TransactionConnectionHelper::totalFailedConnections() const
{
    return m_totalConnectionsFailed.load();
}

std::size_t TransactionConnectionHelper::connectedConnections() const
{
    QnMutexLocker lk(&m_mutex);
    return m_connectedConnections.size();
}

OnConnectionBecomesActiveSubscription& 
    TransactionConnectionHelper::onConnectionBecomesActiveSubscription()
{
    return m_onConnectionBecomesActiveSubscription;
}

OnConnectionFailureSubscription&
    TransactionConnectionHelper::onConnectionFailureSubscription()
{
    return m_onConnectionFailureSubscription;
}

ec2::ApiPeerData TransactionConnectionHelper::localPeer() const
{
    return ec2::ApiPeerData(
        m_moduleGuid,
        m_runningInstanceGuid,
        Qn::PT_Server);
}

void TransactionConnectionHelper::onTransactionConnectionStateChanged(
    ec2::QnTransactionTransportBase* connection,
    ec2::QnTransactionTransportBase::State newState)
{
    const auto connectionId =
        static_cast<test::TransactionTransport*>(connection)->connectionId();

    switch (newState)
    {
        case ec2::QnTransactionTransportBase::Connected:
            moveConnectionToReadyForStreamingState(connection);

        case ec2::QnTransactionTransportBase::NeedStartStreaming:
        case ec2::QnTransactionTransportBase::ReadyForStreaming:
            {
                QnMutexLocker lk(&m_mutex);
                m_connectedConnections.insert(connectionId);
            }
            m_onConnectionBecomesActiveSubscription.notify(newState);
            break;

        case ec2::QnTransactionTransportBase::Error:
        case ec2::QnTransactionTransportBase::Closed:
            ++m_totalConnectionsFailed;
            if (m_removeConnectionAfterClosure)
                removeConnection(connection);
            m_onConnectionFailureSubscription.notify(newState);
            break;

        default:
            break;
    }

    m_condition.wakeAll();
}

void TransactionConnectionHelper::moveConnectionToReadyForStreamingState(
    ec2::QnTransactionTransportBase* connection)
{
    connection->post(
        [connection]()
        {
            connection->setState(ec2::QnTransactionTransportBase::ReadyForStreaming);
            connection->startListening();
        });
}

void TransactionConnectionHelper::removeConnection(
    ec2::QnTransactionTransportBase* connection)
{
    const auto connectionId = getConnectionId(connection);

    connection->post(
        [this, connectionId]()
        {
            QnMutexLocker lk(&m_mutex);
            m_connections.erase(connectionId);
            m_connectedConnections.erase(connectionId);
        });
}

TransactionConnectionHelper::ConnectionId 
    TransactionConnectionHelper::getConnectionId(
        ec2::QnTransactionTransportBase* connection) const
{
    return static_cast<test::TransactionTransport*>(connection)->connectionId();
}

} // namespace test
} // namespace cdb
} // namespace nx
