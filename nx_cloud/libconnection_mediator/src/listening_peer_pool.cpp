/**********************************************************
* Jan 12, 2016
* akolesnikov
***********************************************************/

#include "listening_peer_pool.h"

#include <nx/utils/log/log.h>


namespace nx {
namespace hpm {


////////////////////////////////////////////////////////////
//// class ConstDataLocker
////////////////////////////////////////////////////////////

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


////////////////////////////////////////////////////////////
//// class DataLocker
////////////////////////////////////////////////////////////

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

ListeningPeerPool::DataLocker::DataLocker(ListeningPeerPool::DataLocker&& rhs)
:
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


////////////////////////////////////////////////////////////
//// class ListeningPeerPool
////////////////////////////////////////////////////////////

ListeningPeerPool::DataLocker ListeningPeerPool::insertAndLockPeerData(
    const ConnectionStrongRef& connection,
    const MediaserverData& peerData)
{
    QnMutexLocker lk(&m_mutex);

    const auto peerIterAndInsertionFlag = m_peers.emplace(peerData, ListeningPeerData());
    const auto peerIter = peerIterAndInsertionFlag.first;
    if (peerIterAndInsertionFlag.second)
    {
        peerIter->second.isLocal = true;
        peerIter->second.isListening = false;
        peerIter->second.hostName = peerIter->first.hostName();
    }
    
    const auto curConnectionStrongRef = peerIter->second.peerConnection.lock();
    if (curConnectionStrongRef != connection)
    {
        //detaching from old connection

        //binding to a new connection 
        peerIter->second.peerConnection = connection;
        connection->addOnConnectionCloseHandler([this, peerData, connection]()
        {
            QnMutexLocker lk(&m_mutex);
            const auto peerIter = m_peers.find(peerData);
            if (peerIter == m_peers.end())
                return;

            const auto peerConnectionStrongRef = peerIter->second.peerConnection.lock();
            if (peerConnectionStrongRef != connection)
                return; //peer has been bound to another connection
            NX_LOGX(lit("Peer %1 has disconnected").
                arg(QString::fromUtf8(peerIter->first.hostName())),
                cl_logDEBUG1);
            m_peers.erase(peerIter);
        });
    }

    return DataLocker(
        std::move(lk),
        peerIter);
}

boost::optional<ListeningPeerPool::ConstDataLocker> 
    ListeningPeerPool::findAndLockPeerDataByHostName(const nx::String& hostName) const
{
    QnMutexLocker lk(&m_mutex);

    // TODO: hostName alias resolution in cloud_db

    const auto ids = hostName.split('.');
    if (ids.size() != 2)
        return boost::none;

    //auto-server resolve by system name is forbidden for now to avoid auto-reconnection to another server

    auto peerIter = m_peers.find(MediaserverData(ids[1], ids[0]));  //serverID.systemID
    if (peerIter == m_peers.end())
        return boost::none;

    return ListeningPeerPool::ConstDataLocker(
        std::move(lk),
        std::move(peerIter));
}

std::vector<MediaserverData> ListeningPeerPool::findPeersBySystemId(
    const nx::String& systemId) const
{
    std::vector<MediaserverData> foundPeers;

    QnMutexLocker lk(&m_mutex);
    for (auto it = m_peers.lower_bound(MediaserverData(systemId));
         it != m_peers.end() && it->first.systemId == systemId; ++it)
    {
        foundPeers.push_back(it->first);
    }

    return std::move(foundPeers);
}

}   //hpm
}   //nx
