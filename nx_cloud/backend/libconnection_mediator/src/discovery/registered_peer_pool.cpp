#include "registered_peer_pool.h"

#include <nx/fusion/serialization/json.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace cloud {
namespace discovery {

static constexpr std::size_t kReadBufferSize = 4*1024;

RegisteredPeerPool::RegisteredPeerPool(const conf::Discovery& configuration):
    m_configuration(configuration)
{
}

RegisteredPeerPool::~RegisteredPeerPool()
{
    decltype(m_peerIdToConnection) peerIdToConnection;
    {
        QnMutexLocker locker(&m_mutex);
        m_peerIdToConnection.swap(peerIdToConnection);
    }

    for (const auto& peerIdAndContext: peerIdToConnection)
        peerIdAndContext.second->connection->pleaseStopSync();
}

void RegisteredPeerPool::addPeerConnection(
    const std::string& peerId,
    std::unique_ptr<nx::network::WebSocket> connection)
{
    using namespace std::placeholders;

    QnMutexLocker locker(&m_mutex);

    auto peerContext = std::make_unique<PeerContext>();
    peerContext->readBuffer.reserve(kReadBufferSize);
    peerContext->connection = std::move(connection);

    peerContext->connection->readSomeAsync(
        &peerContext->readBuffer,
        std::bind(&RegisteredPeerPool::onBytesRead, this, peerId, _1, _2));

    m_peerIdToConnection.emplace(peerId, std::move(peerContext));
}

std::vector<std::string> RegisteredPeerPool::getPeerInfoListByType(
    const std::string& peerTypeName) const
{
    QnMutexLocker locker(&m_mutex);

    std::vector<std::string> peersOfRequestedTypes;
    for (const auto& peerContext: m_peerIdToConnection)
    {
        if (!peerContext.second->info)
            continue;
        if (peerContext.second->info->type != peerTypeName)
            continue;
        peersOfRequestedTypes.push_back(peerContext.second->moduleInformationJson);
    }

    return peersOfRequestedTypes;
}

std::vector<std::string> RegisteredPeerPool::getPeerInfoList() const
{
    QnMutexLocker locker(&m_mutex);

    std::vector<std::string> peerInformation;
    for (const auto& peerContext: m_peerIdToConnection)
        peerInformation.push_back(peerContext.second->moduleInformationJson);

    return peerInformation;
}

boost::optional<std::string> RegisteredPeerPool::getPeerInfo(
    const std::string& peerId) const
{
    QnMutexLocker locker(&m_mutex);

    auto peerIter = m_peerIdToConnection.find(peerId);
    if (peerIter == m_peerIdToConnection.end())
        return boost::none;
    if (peerIter->second->moduleInformationJson.empty())
        return boost::none;
    return peerIter->second->moduleInformationJson;
}

void RegisteredPeerPool::onBytesRead(
    const std::string& peerId,
    SystemError::ErrorCode systemErrorCode,
    std::size_t bytesRead)
{
    using namespace std::placeholders;

    QnMutexLocker locker(&m_mutex);

    auto peerIter = m_peerIdToConnection.find(peerId);
    if (peerIter == m_peerIdToConnection.end())
        return; //< Socket has been removed.

    if ((systemErrorCode == SystemError::noError && bytesRead == 0) ||
        nx::network::socketCannotRecoverFromError(systemErrorCode))
    {
        NX_VERBOSE(this, lm("Connection from peer %1 has been closed with result code %2")
            .arg(peerId).arg(SystemError::toString(systemErrorCode)));
        // Connection has been closed.
        m_peerIdToConnection.erase(peerIter);
        return;
    }
    PeerContext& peerContext = *peerIter->second;

    if (systemErrorCode == SystemError::noError)
    {
        DummyInstanceInformation info;
        if (!QJson::deserialize(peerContext.readBuffer, &info) ||
            !isPeerInfoValid(peerId, info))
        {
            NX_DEBUG(this, lm("Closing connection from peer %1 after receiving invalid info")
                .arg(peerId));
            m_peerIdToConnection.erase(peerIter);
            return;
        }

        peerContext.info = std::move(info);
        peerContext.moduleInformationJson = peerContext.readBuffer.toStdString();
    }
    peerContext.readBuffer.clear();

    peerContext.readBuffer.reserve(kReadBufferSize);
    peerContext.connection->readSomeAsync(
        &peerContext.readBuffer,
        std::bind(&RegisteredPeerPool::onBytesRead, this, peerId, _1, _2));
}

bool RegisteredPeerPool::isPeerInfoValid(
    const std::string& peerId,
    const DummyInstanceInformation& info) const
{
    // TODO: #ak Add module type control.
    return info.id == peerId;
}

} // namespace discovery
} // namespace cloud
} // namespace nx
