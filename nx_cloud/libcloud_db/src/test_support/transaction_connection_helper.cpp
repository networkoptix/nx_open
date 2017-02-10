#include "transaction_connection_helper.h"

#include <chrono>

#include <nx/utils/std/cpp14.h>

namespace nx {
namespace cdb {

TransactionConnectionHelper::TransactionConnectionHelper():
    m_moduleGuid(QnUuid::createUuid()),
    m_runningInstanceGuid(QnUuid::createUuid()),
    m_transactionConnectionIdSequence(0)
{
}

TransactionConnectionHelper::~TransactionConnectionHelper()
{
    closeAllConnections();
    m_aioTimer.pleaseStopSync();
}

TransactionConnectionHelper::ConnectionId 
    TransactionConnectionHelper::establishTransactionConnection(
        const QUrl& appserver2BaseUrl,
        const std::string& login,
        const std::string& password,
        KeepAlivePolicy keepAlivePolicy)
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
            login,
            password);
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
    const ConnectionId connectionId = ++m_transactionConnectionIdSequence;
    m_connections.emplace(
        connectionId,
        std::move(connectionContext));
    QUrl url = appserver2BaseUrl;
    url.setPath("/cdb/ec2/events");
    transactionConnectionPtr->doOutgoingConnect(url);

    return connectionId;
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
    if (newState == ec2::QnTransactionTransportBase::Connected)
    {
        // Connection events are reported under lock.
        m_aioTimer.post(
            [connection]()
            {
                connection->setState(ec2::QnTransactionTransportBase::ReadyForStreaming);
                connection->startListening();
            });
    }

    m_condition.wakeAll();
}

} // namespace cdb
} // namespace nx
