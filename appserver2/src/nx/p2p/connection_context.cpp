#include "connection_context.h"
#include <transaction/transaction.h>

namespace nx {
namespace p2p {

bool ConnectionContext::isRemotePeerSubscribedTo(const ec2::ApiPersistentIdData& peer) const
{
    return remoteSubscription.values.contains(peer);
}

bool ConnectionContext::isRemotePeerSubscribedTo(const QnUuid& id) const
{
    ec2::ApiPersistentIdData peer(id, QnUuid());
    auto itr = remoteSubscription.values.lowerBound(peer);
    return itr != remoteSubscription.values.end() && itr.key().id == id;
}

bool ConnectionContext::isLocalPeerSubscribedTo(const ec2::ApiPersistentIdData& peer) const
{
    auto itr = std::find(localSubscription.begin(), localSubscription.end(), peer);
    return itr != localSubscription.end();
}

bool ConnectionContext::updateSequence(const ec2::QnAbstractTransaction& tran)
{
    NX_ASSERT(!sendDataInProgress);
    const ec2::ApiPersistentIdData peerId(tran.peerID, tran.persistentInfo.dbID);
    auto itr = remoteSubscription.values.find(peerId);
    if (itr == remoteSubscription.values.end())
        return false;
    if (tran.persistentInfo.sequence > itr.value())
    {
        itr.value() = tran.persistentInfo.sequence;
        return true;
    }
    return false;
}

ec2::ApiPersistentIdData ConnectionContext::decode(PeerNumberType shortPeerNumber) const
{
    return shortPeerInfo.decode(shortPeerNumber);
}

PeerNumberType ConnectionContext::encode(const ec2::ApiPersistentIdData& fullId, PeerNumberType shortPeerNumber)
{
    return shortPeerInfo.encode(fullId, shortPeerNumber);
}

} // namespace p2p
} // namespace nx
