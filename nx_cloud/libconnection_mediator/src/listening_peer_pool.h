/**********************************************************
* Jan 12, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <memory>

#include <boost/optional.hpp>

#include <nx/network/stun/abstract_server_connection.h>
#include <nx/utils/thread/mutex.h>


namespace nx {
namespace hpm {

struct ListeningPeerData
{
    /** \a true, if peer listens on this mediator instance */
    bool isLocal;
    nx::String hostname;
    /** valid for locally-registered peer only */
    std::weak_ptr<stun::AbstractServerConnection> peerConnection;
};

/** Stores information on all peers currently listening for connections on all mediator instances.
    For peers listening locally, holds weak reference to the connection.
    \note synchronizes listening peer list with other mediator instances
    \note class is thread-safe
 */
class ListeningPeerPool
{
public:
    void add(
        nx::String hostname,
        std::weak_ptr<stun::AbstractServerConnection> peerConnection);
    void remove(const nx::String& hostname);
    /**
        If peer with \a hostname listens on multiple mediator instances, then\n
            - local peer is preferred
            - if not available locally, random remote instance is selected
    */
    boost::optional<ListeningPeerData> find(const nx::String& hostname) const;

private:
    QnMutex m_mutex;
};

}   //hpm
}   //nx
