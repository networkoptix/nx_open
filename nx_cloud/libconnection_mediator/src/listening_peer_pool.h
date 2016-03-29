/**********************************************************
* Jan 12, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <memory>

#include <boost/optional.hpp>

#include <nx/network/cloud/data/connection_method.h>
#include <nx/network/stun/abstract_server_connection.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/singleton.h>

#include "request_processor.h"
#include "server/stun_request_processing_helper.h"


namespace nx {
namespace hpm {

struct ListeningPeerData
{
    /** \a true, if peer listens on this mediator instance */
    bool isLocal;
    bool isListening;
    nx::String hostName;
    /** valid for locally-registered peer only */
    ConnectionWeakRef peerConnection;
    std::list< SocketAddress > endpoints;
    api::ConnectionMethods connectionMethods;

    ListeningPeerData()
    :
        isLocal(true),
        isListening(false),
        connectionMethods(0)
    {
    }
};

/** Stores information on all peers currently listening for connections on all mediator instances.
    For peers listening locally, holds weak reference to the connection.
    \note synchronizes listening peer list with other mediator instances
    \note Peer entry is removed from this pool when connection has been closed
    \note class is thread-safe
 */
class ListeningPeerPool
:
    public Singleton<ListeningPeerPool> //needed for unit tests only
{
    typedef std::map< MediaserverData, ListeningPeerData > PeerContainer;

public:
    class ConstDataLocker
    {
        friend class ListeningPeerPool;

    public:
        ConstDataLocker(ConstDataLocker&&);
        ConstDataLocker& operator=(ConstDataLocker&&);

        const MediaserverData& key() const;
        const ListeningPeerData& value() const;

    private:
        QnMutexLockerBase m_locker;
        PeerContainer::const_iterator m_peerIter;

        ConstDataLocker(
            QnMutexLockerBase locker,
            PeerContainer::const_iterator peerIter);
    };

    class DataLocker
    :
        public ConstDataLocker
    {
        friend class ListeningPeerPool;

    public:
        DataLocker(DataLocker&&);
        DataLocker& operator=(DataLocker&&);

        ListeningPeerData& value();

    private:
        PeerContainer::iterator m_peerIter;

        DataLocker(
            QnMutexLockerBase locker,
            PeerContainer::iterator peerIter);
    };

    /** Inserts new or returns existing element with key \a peerData.
        \note Another thread will block trying to lock \a peerData while
            \a DataLocker instance is still alive
     */
    DataLocker insertAndLockPeerData(
        const ConnectionStrongRef& connection,
        const MediaserverData& peerData);

    boost::optional<ConstDataLocker> findAndLockPeerDataByHostName(
        const nx::String& hostName) const;

    std::vector<MediaserverData> findPeersBySystemId(
        const nx::String& systemId) const;

private:
    mutable QnMutex m_mutex;
    PeerContainer m_peers;
};

}   //hpm
}   //nx
