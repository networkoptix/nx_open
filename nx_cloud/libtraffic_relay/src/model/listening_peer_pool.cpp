#include "listening_peer_pool.h"

namespace nx {
namespace cloud {
namespace relay {
namespace model {

ListeningPeerPool::ListeningPeerPool():
    m_terminated(false)
{
}

ListeningPeerPool::~ListeningPeerPool()
{
    m_apiCallCounter.wait();

    m_unsuccessfulResultReporter.pleaseStopSync();

    {
        QnMutexLocker lock(&m_mutex);
        m_terminated = true;
    }

    for (auto& connectionContext: m_peerNameToConnection)
        connectionContext.second.connection->pleaseStopSync();
}

void ListeningPeerPool::addConnection(
    std::string peerName,
    std::unique_ptr<AbstractStreamSocket> connection)
{
    QnMutexLocker lock(&m_mutex);
    ConnectionContext connectionContext;
    connectionContext.connection = std::move(connection);
    auto it = m_peerNameToConnection.emplace(
        std::move(peerName), std::move(connectionContext));
    monitoringConnectionForClosure(it->first, &it->second);
}

std::size_t ListeningPeerPool::getConnectionCountByPeerName(
    const std::string& peerName) const
{
    QnMutexLocker lock(&m_mutex);
    return m_peerNameToConnection.count(peerName);
}

bool ListeningPeerPool::isPeerListening(const std::string& peerName) const
{
    return getConnectionCountByPeerName(peerName) > 0;
}

void ListeningPeerPool::takeIdleConnection(
    const std::string& peerName,
    ListeningPeerPool::TakeIdleConnection completionHandler)
{
    QnMutexLocker lock(&m_mutex);

    auto peerConnectionIter = m_peerNameToConnection.find(peerName);
    if (peerConnectionIter == m_peerNameToConnection.end())
    {
        m_unsuccessfulResultReporter.post(
            [completionHandler = std::move(completionHandler)]()
            {
                completionHandler(api::ResultCode::notFound, nullptr);
            });
        return;
    }

    auto connectionContext = std::move(peerConnectionIter->second);
    m_peerNameToConnection.erase(peerConnectionIter);

    auto connectionPtr = connectionContext.connection.get();
    connectionPtr->post(
        [this, connectionContext = std::move(connectionContext),
            completionHandler = std::move(completionHandler),
            scopedCallGuard = m_apiCallCounter.getScopedIncrement()]() mutable
        {
            giveAwayConnection(
                std::move(connectionContext),
                std::move(completionHandler));
        });
}

void ListeningPeerPool::giveAwayConnection(
    ListeningPeerPool::ConnectionContext connectionContext,
    ListeningPeerPool::TakeIdleConnection completionHandler)
{
    connectionContext.connection->cancelIOSync(network::aio::etNone);
    completionHandler(
        api::ResultCode::ok,
        std::move(connectionContext.connection));
}

void ListeningPeerPool::monitoringConnectionForClosure(
    const std::string& peerName,
    ConnectionContext* connectionContext)
{
    using namespace std::placeholders;

    constexpr int readBufferSize = 1; //< We need only detect connection closure.

    connectionContext->readBuffer.clear();
    connectionContext->readBuffer.reserve(readBufferSize);
    connectionContext->connection->readSomeAsync(
        &connectionContext->readBuffer,
        std::bind(&ListeningPeerPool::onConnectionReadCompletion, this,
            peerName, connectionContext, _1, _2));
}

void ListeningPeerPool::onConnectionReadCompletion(
    const std::string& peerName,
    ConnectionContext* connectionContext,
    SystemError::ErrorCode sysErrorCode,
    std::size_t bytesRead)
{
    const bool isConnectionClosed =
        sysErrorCode == SystemError::noError && bytesRead == 0;

    if (isConnectionClosed ||
        (sysErrorCode != SystemError::noError && 
         nx::network::socketCannotRecoverFromError(sysErrorCode)))
    {
        return closeConnection(peerName, connectionContext, sysErrorCode);
    }

    // TODO: What to do if some data has been received?

    monitoringConnectionForClosure(peerName, connectionContext);
}

void ListeningPeerPool::closeConnection(
    const std::string& peerName,
    ConnectionContext* connectionContext,
    SystemError::ErrorCode /*sysErrorCode*/)
{
    QnMutexLocker lock(&m_mutex);

    if (m_terminated)
        return;

    auto peerConnectionRange = m_peerNameToConnection.equal_range(peerName);
    for (auto connectionIter = peerConnectionRange.first;
        connectionIter != peerConnectionRange.second;
        ++connectionIter)
    {
        if (connectionContext == &connectionIter->second)
        {
            m_peerNameToConnection.erase(connectionIter);
            break;
        }
    }
}

} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
