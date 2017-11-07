#include "transaction_connection_helper.h"

#include <chrono>

#include <QUrlQuery>

#include <nx/utils/std/cpp14.h>
#include <nx/utils/random.h>

#include <nx/cloud/cdb/api/ec2_request_paths.h>

namespace nx {
namespace cdb {
namespace test {

TransactionConnectionHelper::TransactionConnectionHelper():
    m_moduleGuid(QnUuid::createUuid()),
    m_runningInstanceGuid(QnUuid::createUuid()),
    m_totalConnectionsFailed(0),
    m_transactionConnectionIdSequence(0),
    m_removeConnectionAfterClosure(false),
    m_maxDelayBeforeConnect(std::chrono::milliseconds::zero())
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

void TransactionConnectionHelper::setMaxDelayBeforeConnect(
    std::chrono::milliseconds delay)
{
    m_maxDelayBeforeConnect = delay;
}

TransactionConnectionHelper::ConnectionId
    TransactionConnectionHelper::establishTransactionConnection(
        const nx::utils::Url& appserver2BaseUrl,
        const std::string& login,
        const std::string& password,
        KeepAlivePolicy keepAlivePolicy,
        int protocolVersion,
        const QnUuid& peerId)
{
    ConnectionContext connectionContext = prepareConnectionContext(
        login, password, keepAlivePolicy, protocolVersion, peerId);

    QnMutexLocker lock(&m_mutex);

    startConnection(&connectionContext, appserver2BaseUrl);

    const auto connectionId = connectionContext.connection->connectionId();
    m_connections.emplace(
        connectionId,
        std::move(connectionContext));

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
    {
        connectionContext.second.timer->pleaseStopSync();
        connectionContext.second.connection->pleaseStopSync();
    }
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

TransactionConnectionHelper::ConnectionContext 
    TransactionConnectionHelper::prepareConnectionContext(
        const std::string& login,
        const std::string& password,
        KeepAlivePolicy keepAlivePolicy,
        int protocolVersion,
        const QnUuid& peerId)
{
    ConnectionContext connectionContext;
    connectionContext.peerInfo = localPeer();
    connectionContext.peerInfo.id = peerId.isNull() ? QnUuid::createUuid() : peerId;
    connectionContext.connectionGuardSharedState =
        std::make_unique<::ec2::ConnectionGuardSharedState>();
    connectionContext.connection =
        std::make_unique<test::TransactionTransport>(
            connectionContext.connectionGuardSharedState.get(),
            connectionContext.peerInfo,
            login,
            password,
            protocolVersion,
            ++m_transactionConnectionIdSequence);
    connectionContext.timer = std::make_unique<nx::network::aio::Timer>();
    connectionContext.timer->bindToAioThread(m_aioTimer.getAioThread());
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

    return connectionContext;
}

void TransactionConnectionHelper::startConnection(
    ConnectionContext* connectionContext,
    const nx::utils::Url& appserver2BaseUrl)
{
    // TODO: #ak TransactionTransport should do that. But somehow it doesn't...
    const auto targetUrl = prepareTargetUrl(appserver2BaseUrl, connectionContext->peerInfo.id);

    if (m_maxDelayBeforeConnect > std::chrono::milliseconds::zero())
    {
        const auto delay = std::chrono::milliseconds(
            nx::utils::random::number<int>(0, m_maxDelayBeforeConnect.count()));
        connectionContext->timer->start(
            delay,
            [targetUrl, connection = connectionContext->connection.get()]()
            {
                connection->doOutgoingConnect(targetUrl);
            });
    }
    else
    {
        connectionContext->connection->doOutgoingConnect(targetUrl);
    }
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
            /*fallthrough*/
        case ec2::QnTransactionTransportBase::NeedStartStreaming:
        case ec2::QnTransactionTransportBase::ReadyForStreaming:
            // Transaction transport invokes this handler with mutex locked, 
            // so have to do some work around this misbehavior.
            m_aioTimer.post(
                [this, connectionId]()
                {
                    QnMutexLocker lk(&m_mutex);
                    m_connectedConnections.insert(connectionId);
                });
            m_onConnectionBecomesActiveSubscription.notify(connectionId, newState);
            break;

        case ec2::QnTransactionTransportBase::Error:
        case ec2::QnTransactionTransportBase::Closed:
            ++m_totalConnectionsFailed;
            if (m_removeConnectionAfterClosure)
                removeConnection(connection);
            m_onConnectionFailureSubscription.notify(connectionId, newState);
            break;

        default:
            break;
    }

    m_condition.wakeAll();
}

nx::utils::Url TransactionConnectionHelper::prepareTargetUrl(
    const nx::utils::Url& appserver2BaseUrl, const QnUuid& localPeerId)
{
    nx::utils::Url url = appserver2BaseUrl;
    QUrlQuery urlQuery(url.query());
    urlQuery.addQueryItem("guid", localPeerId.toString());
    url.setQuery(urlQuery);
    url.setPath(api::kEc2EventsPath);
    return url;
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
