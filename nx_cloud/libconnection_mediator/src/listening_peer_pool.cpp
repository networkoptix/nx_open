/**********************************************************
* Jan 12, 2016
* akolesnikov
***********************************************************/

#include "listening_peer_pool.h"


namespace nx {
namespace hpm {

void ListeningPeerPool::add(
    nx::String hostname,
    std::weak_ptr<stun::AbstractServerConnection> peerConnection)
{
    //TODO #ak
}

void ListeningPeerPool::remove(const nx::String& hostname)
{
    //TODO #ak
}

boost::optional<ListeningPeerData> ListeningPeerPool::find(const nx::String& hostname) const
{
    //TODO #ak
    return boost::optional<ListeningPeerData>();
}

}   //hpm
}   //nx
