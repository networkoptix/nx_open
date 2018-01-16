#include "listening_peer_pool.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace hpm {

QString ListeningPeerData::toString() const
{
    QStringList opts;
    if (isLocal) opts << lm("local");
    if (isListening) opts << lm("listening");
    if (endpoints.size()) opts << lm("endpoints=%1").container(endpoints);
    if (connectionMethods) opts << lm("methods=%1")
        .arg(api::ConnectionMethod::toString(connectionMethods));

    return lm("%1<%2>").arg(hostName).container(opts);
}

//-------------------------------------------------------------------------------------------------
// class ConstDataLocker

ListeningPeerPool::ConstDataLocker::ConstDataLocker(
    QnMutexLockerBase locker,
    PeerContainer::const_iterator peerIter)
:
    m_locker(std::move(locker)),
    m_peerIter(std::move(peerIter))
{
}

ListeningPeerPool::ConstDataLocker::ConstDataLocker(ConstDataLocker&& rhs)
:
    m_locker(std::move(rhs.m_locker)),
    m_peerIter(std::move(rhs.m_peerIter))
{
}

ListeningPeerPool::ConstDataLocker& ListeningPeerPool::ConstDataLocker::operator=(
    ListeningPeerPool::ConstDataLocker&& rhs)
{
    if (this != &rhs)
    {
        m_locker = std::move(rhs.m_locker);
        m_peerIter = std::move(rhs.m_peerIter);
    }

    return *this;
}

const MediaserverData& ListeningPeerPool::ConstDataLocker::key() const
{
    return m_peerIter->first;
}

const ListeningPeerData& ListeningPeerPool::ConstDataLocker::value() const
{
    return m_peerIter->second;
}


//-------------------------------------------------------------------------------------------------
// class DataLocker

ListeningPeerPool::DataLocker::DataLocker(
    QnMutexLockerBase locker,
    PeerContainer::iterator peerIter)
:
    ConstDataLocker(
        std::move(locker),
        peerIter),
    m_peerIter(std::move(peerIter))
{
}

ListeningPeerPool::DataLocker::DataLocker(ListeningPeerPool::DataLocker&& rhs):
    ConstDataLocker(std::move(rhs)),
    m_peerIter(std::move(rhs.m_peerIter))
{
}

ListeningPeerPool::DataLocker& ListeningPeerPool::DataLocker::operator=(
    ListeningPeerPool::DataLocker&& rhs)
{
    if (this != &rhs)
    {
        this->ListeningPeerPool::ConstDataLocker::operator=(
            std::move(static_cast<ListeningPeerPool::ConstDataLocker&>(rhs)));
        m_peerIter = std::move(rhs.m_peerIter);
    }

    return *this;
}

ListeningPeerData& ListeningPeerPool::DataLocker::value()
{
    return m_peerIter->second;
}


//-------------------------------------------------------------------------------------------------
// class ListeningPeerPool

ListeningPeerPool::ListeningPeerPool(const conf::ListeningPeer& settings):
    m_settings(settings)
{
}

ListeningPeerPool::~ListeningPeerPool()
{
    m_counter.wait();

    PeerContainer peers;
    {
        QnMutexLocker lock(&m_mutex);
        m_peers.swap(peers);
    }
    peers.clear();
}

ListeningPeerPool::DataLocker ListeningPeerPool::insertAndLockPeerData(
    const std::shared_ptr<nx::stun::ServerConnection>& connection,
    const MediaserverData& peerData)
{
    QnMutexLocker lock(&m_mutex);

    const auto peerIterAndInsertionFlag = m_peers.emplace(peerData, ListeningPeerData());
    const auto peerIter = peerIterAndInsertionFlag.first;
    if (peerIterAndInsertionFlag.second)
    {
        peerIter->second.isLocal = true;
        peerIter->second.isListening = false;
        peerIter->second.hostName = peerIter->first.hostName();
    }

    if (peerIter->second.peerConnection != connection)
    {
        if (peerIter->second.peerConnection)
            closeConnectionAsync(std::move(peerIter->second.peerConnection));

        // Binding to a new connection.
        peerIter->second.peerConnection = connection;
        connection->setInactivityTimeout(
            m_settings.connectionInactivityTimeout);
        connection->addOnConnectionCloseHandler([this, peerData, connectionPtr = connection.get()]()
        {
            QnMutexLocker lock(&m_mutex);
            const auto peerIter = m_peers.find(peerData);
            if (peerIter == m_peers.end())
                return;

            if (peerIter->second.peerConnection.get() != connectionPtr)
                return; //peer has been bound to another connection
            NX_LOGX(lit("Peer %1 has disconnected").
                arg(QString::fromUtf8(peerIter->first.hostName())),
                cl_logDEBUG1);
            m_peers.erase(peerIter);
        });
    }

    return DataLocker(
        std::move(lock),
        peerIter);
}

boost::optional<ListeningPeerPool::ConstDataLocker>
    ListeningPeerPool::findAndLockPeerDataByHostName(const nx::String& hostName) const
{
    QnMutexLocker lock(&m_mutex);

    // TODO: hostName alias resolution in cloud_db

    const auto ids = hostName.split('.');
    if (ids.size() == 2)
    {
        //hostName is serverId.systemId
        const auto& systemId = ids[1];
        const auto& serverId = ids[0];

        //searching for exact server match
        auto peerIter = m_peers.find(MediaserverData(systemId, serverId));
        if (peerIter != m_peers.end())  //found exact match
            return ListeningPeerPool::ConstDataLocker(
                std::move(lock),
                std::move(peerIter));
    }
    else if (ids.size() == 1)
    {
        const auto& systemId = ids[0];
        //resolving to any server of a system
        auto peerIter = m_peers.lower_bound(MediaserverData(systemId, nx::String()));
        if (peerIter != m_peers.end() && peerIter->first.systemId == systemId)
            return ListeningPeerPool::ConstDataLocker(
                std::move(lock),
                std::move(peerIter));
    }

    return boost::none;
}

std::vector<MediaserverData> ListeningPeerPool::findPeersBySystemId(
    const nx::String& systemId) const
{
    std::vector<MediaserverData> foundPeers;

    QnMutexLocker lock(&m_mutex);
    for (auto it = m_peers.lower_bound(MediaserverData(systemId));
         it != m_peers.end() && it->first.systemId == systemId; ++it)
    {
        foundPeers.push_back(it->first);
    }

    return std::move(foundPeers);
}

data::ListeningPeersBySystem ListeningPeerPool::getListeningPeers() const
{
    data::ListeningPeersBySystem result;

    QnMutexLocker lock(&m_mutex);
    for (const auto& peerPair: m_peers)
    {
        data::ListeningPeer peerData;
        peerData.connectionEndpoint =
            peerPair.second.peerConnection->getSourceAddress().toString().toUtf8();

        for (const auto& forwardedEndpoint: peerPair.second.endpoints)
            peerData.directTcpEndpoints.push_back(forwardedEndpoint.toString());

        auto& system = result[peerPair.first.systemId];
        system.emplace(peerPair.first.serverId, std::move(peerData));
    }

    return result;
}

std::vector<ConnectionWeakRef> ListeningPeerPool::getAllConnections() const
{
    QnMutexLocker lock(&m_mutex);
    std::vector<ConnectionWeakRef> connections;
    for (const auto& peer: m_peers)
        connections.push_back(peer.second.peerConnection);

    return connections;
}

void ListeningPeerPool::closeConnectionAsync(
    std::shared_ptr<nx::stun::ServerConnection> peerConnection)
{
    auto peerConnectionPtr = peerConnection.get();
    peerConnectionPtr->pleaseStop(
        [peerConnection = std::move(peerConnection),
            lock = m_counter.getScopedIncrement()]() mutable
        {
            peerConnection->closeConnection(SystemError::connectionReset);
            peerConnection.reset();
        });
}

} // namespace hpm
} // namespace nx
