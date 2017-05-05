#include "listening_peer_pool.h"

#include <nx/network/socket_common.h>
#include <nx/utils/std/algorithm.h>

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
    const std::string& peerNameOriginal,
    std::unique_ptr<AbstractStreamSocket> connection)
{
    auto peerName = utils::reverseWords(peerNameOriginal, ".");

    QnMutexLocker lock(&m_mutex);
    ConnectionContext connectionContext;
    connectionContext.connection = std::move(connection);
    auto it = m_peerNameToConnection.emplace(
        std::move(peerName), std::move(connectionContext));
    monitoringConnectionForClosure(it->first, &it->second);
}

std::size_t ListeningPeerPool::getConnectionCountByPeerName(
    const std::string& peerNameOriginal) const
{
    auto peerName = utils::reverseWords(peerNameOriginal, ".");

    QnMutexLocker lock(&m_mutex);
    return utils::countElementsWithPrefix(m_peerNameToConnection, peerName);
}

bool ListeningPeerPool::isPeerListening(const std::string& peerNameOriginal) const
{
    return getConnectionCountByPeerName(peerNameOriginal) > 0;
}

std::string ListeningPeerPool::findListeningPeerByDomain(
    const std::string& domainName) const
{
    auto domainNameReversed = utils::reverseWords(domainName, ".");

    QnMutexLocker lock(&m_mutex);
    auto it = utils::findAnyElementWithPrefix(m_peerNameToConnection, domainNameReversed);
    if (it == m_peerNameToConnection.end())
        return std::string();
    return utils::reverseWords(it->first, ".");
}

void ListeningPeerPool::takeIdleConnection(
    const std::string& peerNameOriginal,
    ListeningPeerPool::TakeIdleConnection completionHandler)
{
    QnMutexLocker lock(&m_mutex);

    auto peerName = utils::reverseWords(peerNameOriginal, ".");

    auto peerConnectionIter = utils::findAnyElementWithPrefix(m_peerNameToConnection, peerName);
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

    // TODO: What if some data has been received?

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

    auto peerConnectionRange = utils::equalRangeByPrefix(m_peerNameToConnection, peerName);
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
