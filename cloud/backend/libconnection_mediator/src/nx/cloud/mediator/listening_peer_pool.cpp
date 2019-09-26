#include "listening_peer_pool.h"

#include <nx/utils/log/log.h>

#include <nx/cloud/mediator/listening_peer_db.h>

namespace nx {
namespace hpm {

namespace {

MediaserverData toLowerCase(const MediaserverData& data)
{
    return MediaserverData(
        data.systemId.toLower(),
        data.serverId.toLower());
}

MediaserverData toMediaServerData(const nx::hpm::api::PeerConnectionSpeed& connectionSpeed)
{
    return toLowerCase(
        MediaserverData(
            connectionSpeed.systemId.c_str(),
            connectionSpeed.serverId.c_str()));
}

} // namespace

QString ListeningPeerData::toString() const
{
    QStringList opts;
    if (isLocal)
        opts << lm("local");
    if (isListening)
        opts << lm("listening");
    if (endpoints.size())
        opts << lm("endpoints=%1").container(endpoints);
    opts << lm("cloudConnectVersion=%1").args(static_cast<int>(cloudConnectVersion));

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

ListeningPeerPool::ListeningPeerPool(
    const conf::ListeningPeer& settings,
    ListeningPeerDb* listeningPeerDb)
    :
    m_settings(settings),
    m_listeningPeerDb(listeningPeerDb)
{
    m_uplinkSpeedUpdatedId =
        m_listeningPeerDb->subscribeToUplinkSpeedUpdated(
        std::bind(&ListeningPeerPool::onUplinkSpeedUpdated, this, std::placeholders::_1));
}

ListeningPeerPool::~ListeningPeerPool()
{
    m_listeningPeerDb->unsubscribeFromUplinkSpeedUpdated(m_uplinkSpeedUpdatedId);

    m_counter.wait();

    PeerContainer peers;
    {
        QnMutexLocker lock(&m_mutex);
        m_peers.swap(peers);
    }

    for (const auto& peer: peers)
    {
        // TODO: #ak Get rid of std::shared_ptr here.
        if (peer.second.peerConnection.use_count() == 1)
            peer.second.peerConnection->pleaseStopSync();
    }

    peers.clear();
    m_asyncOperationGuard->terminate();
}

ListeningPeerPool::DataLocker ListeningPeerPool::insertAndLockPeerData(
    const std::shared_ptr<nx::network::stun::ServerConnection>& connection,
    const MediaserverData& peerData)
{
    QnMutexLocker lock(&m_mutex);

    NX_VERBOSE(this, "Saving new connection (%1) from peer %2", (void*) connection.get(), peerData);

    const auto peerIterAndInsertionFlag =
        m_peers.emplace(toLowerCase(peerData), ListeningPeerData());
    const auto peerIter = peerIterAndInsertionFlag.first;
    if (peerIterAndInsertionFlag.second)
    {
        NX_DEBUG(this, "Peer %1 is now online", peerData.hostName());

        peerIter->second.isLocal = true;
        peerIter->second.isListening = false;
        peerIter->second.hostName = peerIter->first.hostName();
        m_listeningPeerDb->addPeer(
            peerData.hostName().toStdString(),
            [hostName = peerData.hostName()](bool added)
            {
                // Can't use "this" because the life time of m_listeningPeerDb is longer than "this".
                // this handler with "this" may happen after "this" has been destroyed.
                NX_VERBOSE(typeid(ListeningPeerPool), "Peer %1 added to ListeningPeerDb: %2",
                    hostName, added);
            });
    }

    if (peerIter->second.peerConnection != connection)
    {
        NX_VERBOSE(this, "Replacing old connection (%1) from peer %2 with a new one (%3)",
            (void*) peerIter->second.peerConnection.get(), peerData, (void*) connection.get());

        if (peerIter->second.peerConnection)
            closeConnectionAsync(std::move(peerIter->second.peerConnection));
        // Binding to a new connection.
        peerIter->second.peerConnection = connection;
        connection->setInactivityTimeout(
            m_settings.connectionInactivityTimeout);
        connection->addOnConnectionCloseHandler(
            [this, peerData, connectionPtr = connection.get(),
                guard = m_asyncOperationGuard.sharedGuard()](
                    SystemError::ErrorCode /*closeReason*/)
            {
                // TODO: #ak Get rid of this guard after resolving
                // dependency issue between Controller and STUN server.
                auto lock = guard->lock();
                if (!lock)
                    return; //< ListeningPeerPool has been destroyed.
                onListeningPeerConnectionClosed(peerData, connectionPtr);
            });
    }

    return DataLocker(
        std::move(lock),
        peerIter);
}

boost::optional<ListeningPeerPool::ConstDataLocker>
    ListeningPeerPool::findAndLockPeerDataByHostName(const nx::String& hostName) const
{
    // TODO: hostName alias resolution in cloud_db

    const auto ids = hostName.toLower().split('.');
    if (ids.size() == 2)
    {
        QnMutexLocker lock(&m_mutex);
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

        QnMutexLocker lock(&m_mutex);

        if (!m_bestUplinkSpeed)
            m_bestUplinkSpeed = findLocalPeerWithBestUplinkSpeedUnsafe(systemId);

        if (m_bestUplinkSpeed)
        {
            NX_VERBOSE(this, "Found best uplink speed while searching for Peer: %1",
                *m_bestUplinkSpeed);
            auto peerIter = m_peers.find(toMediaServerData(*m_bestUplinkSpeed));
            if (peerIter != m_peers.end())
                return ConstDataLocker(std::move(lock), std::move(peerIter));
        }

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

    return foundPeers;
}

api::ListeningPeersBySystem ListeningPeerPool::getListeningPeers() const
{
    api::ListeningPeersBySystem result;

    QnMutexLocker lock(&m_mutex);
    for (const auto& peerPair: m_peers)
    {
        api::ListeningPeer peerData;
        peerData.connectionEndpoint =
            peerPair.second.peerConnection->getSourceAddress().toString().toUtf8();

        for (const auto& forwardedEndpoint: peerPair.second.endpoints)
            peerData.directTcpEndpoints.push_back(forwardedEndpoint.toString());

        auto& system = result[peerPair.first.systemId];
        system.emplace(peerPair.first.serverId, std::move(peerData));
    }

    return result;
}

int ListeningPeerPool::listeningPeerCount() const
{
    QnMutexLocker lock(&m_mutex);
    return (int) m_peers.size();
}

std::vector<ConnectionWeakRef> ListeningPeerPool::getAllConnections() const
{
    QnMutexLocker lock(&m_mutex);

    std::vector<ConnectionWeakRef> connections;
    for (const auto& peer: m_peers)
        connections.push_back(peer.second.peerConnection);

    return connections;
}

void ListeningPeerPool::onListeningPeerConnectionClosed(
    const MediaserverData& peerData,
    nx::network::stun::AbstractServerConnection* connection)
{
    QnMutexLocker lock(&m_mutex);

    auto peerDataLowerCase = toLowerCase(peerData);

    const auto peerIter = m_peers.find(peerDataLowerCase);
    if (peerIter == m_peers.end())
        return;

    if (peerIter->second.peerConnection.get() != connection)
        return; //< Peer has been bound to another connection.

    NX_DEBUG(this, "Peer %1 is now offline", peerIter->first.hostName());

    m_peers.erase(peerIter);

    m_listeningPeerDb->removePeer(peerData.hostName().toStdString(),
        [hostName = peerData.hostName()](bool removed)
        {
            // Can't use "this" because the life time of m_listeningPeerDb is longer than "this".
            // this handler with "this" may happen after "this" has been destroyed.
            NX_VERBOSE(typeid(ListeningPeerPool), "Peer %1 removed from ListeningPeerDb: %2",
                hostName, removed);
        });

    if (!m_bestUplinkSpeed ||
        peerDataLowerCase.hostName() == toMediaServerData(*m_bestUplinkSpeed).hostName())
    {
        m_bestUplinkSpeed =
            findLocalPeerWithBestUplinkSpeedUnsafe(peerDataLowerCase.systemId);

        if (m_bestUplinkSpeed)
        {
            NX_VERBOSE(this, "Found best uplink speed after closing connection to Peer: %1",
                *m_bestUplinkSpeed);
        }
    }
}

void ListeningPeerPool::closeConnectionAsync(
    std::shared_ptr<nx::network::stun::ServerConnection> peerConnection)
{
    NX_VERBOSE(this, "Closing connection (%1)", (void*) peerConnection.get());

    auto peerConnectionPtr = peerConnection.get();
    peerConnectionPtr->pleaseStop(
        [peerConnection = std::move(peerConnection),
            lock = m_counter.getScopedIncrement()]() mutable
        {
            peerConnection->closeConnection(SystemError::connectionReset);
            peerConnection.reset();
        });
}

void ListeningPeerPool::onUplinkSpeedUpdated(nx::hpm::api::PeerConnectionSpeed peerUplinkSpeed)
{
    QnMutexLocker lock(&m_mutex);

    // Ignoring non local peers
    if (m_peers.find(toMediaServerData(peerUplinkSpeed)) == m_peers.end())
        return;

    // Default value
    if (!m_bestUplinkSpeed)
    {
        m_bestUplinkSpeed = peerUplinkSpeed;
        return;
    }

    // Ignoring new speed if current is still better.
    if (peerUplinkSpeed.connectionSpeed.bandwidth < m_bestUplinkSpeed->connectionSpeed.bandwidth)
        return;

    m_bestUplinkSpeed = peerUplinkSpeed;

    NX_VERBOSE(this, "Updating best uplink speed: %1", m_bestUplinkSpeed->toString());
}

std::optional<nx::hpm::api::PeerConnectionSpeed>
    ListeningPeerPool::findLocalPeerWithBestUplinkSpeedUnsafe(
        const nx::String& systemId) const
{
    using Status = std::pair<std::string, ListeningPeerStatus>;

    auto statuses = m_listeningPeerDb->getListeningPeerStatus(nx::toStdString(systemId));
    if (statuses.empty())
        return std::nullopt;

    std::vector<Status> statusesSorted;
    std::transform(statuses.begin(), statuses.end(), std::back_inserter(statusesSorted),
        [](Status&& status) { return std::move(status); });

    std::sort(statusesSorted.begin(), statusesSorted.end(),
        [](const Status& a, const Status& b)
        {
            // Sorting in descending order
            return a.second.uplinkSpeed.bandwidth > b.second.uplinkSpeed.bandwidth;
        });

    for (const auto& status : statusesSorted)
    {
        auto peerIter = m_peers.find(
            MediaserverData (
                status.second.systemId.c_str(),
                status.second.serverId.c_str()));

        // 0 bandwidth probably means that uplinkSpeed is default constructed and shouldn't be used
        if (peerIter != m_peers.end() && status.second.uplinkSpeed.bandwidth != 0)
        {
            nx::hpm::api::PeerConnectionSpeed uplinkSpeed{
                status.second.serverId,
                status.second.systemId,
                status.second.uplinkSpeed};

            return uplinkSpeed;
        }
    }

    return std::nullopt;
}

} // namespace hpm
} // namespace nx
