// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connection_context.h"
#include <transaction/transaction.h>

namespace nx {
namespace p2p {

QString toString(UpdateSequenceResult value)
{
    switch (value)
    {
        case UpdateSequenceResult::ok: return "ok";
        case UpdateSequenceResult::notSubscribed: return "notSubscribed";
        case UpdateSequenceResult::alreadyKnown: return "alreadyKnown";
        default: return "unknown";
    }
}

bool ConnectionContext::isRemotePeerSubscribedTo(const vms::api::PersistentIdData& peer) const
{
    return remoteSubscription.values.contains(peer);
}

bool ConnectionContext::isRemotePeerSubscribedTo(const nx::Uuid& id) const
{
    nx::vms::api::PersistentIdData peer(id, nx::Uuid());
    auto itr = remoteSubscription.values.lowerBound(peer);
    return itr != remoteSubscription.values.end() && itr.key().id == id;
}

bool ConnectionContext::isLocalPeerSubscribedTo(const vms::api::PersistentIdData& peer) const
{
    auto itr = std::find(localSubscription.begin(), localSubscription.end(), peer);
    return itr != localSubscription.end();
}

UpdateSequenceResult ConnectionContext::updateSequence(const ec2::QnAbstractTransaction& tran)
{
    NX_ASSERT(!sendDataInProgress);
    const vms::api::PersistentIdData peerId(tran.peerID, tran.persistentInfo.dbID);
    auto itr = remoteSubscription.values.find(peerId);
    if (itr == remoteSubscription.values.end())
    {
        if (!isRemoteSubscribedToAll)
            return UpdateSequenceResult::notSubscribed;
        itr = remoteSubscription.values.insert(peerId, 0);
    }
    if (tran.persistentInfo.sequence > itr.value())
    {
        itr.value() = tran.persistentInfo.sequence;
        return UpdateSequenceResult::ok;
    }
    return UpdateSequenceResult::alreadyKnown;
}

vms::api::PersistentIdData ConnectionContext::decode(PeerNumberType shortPeerNumber) const
{
    return shortPeerInfo.decode(shortPeerNumber);
}

PeerNumberType ConnectionContext::encode(
    const vms::api::PersistentIdData& fullId, PeerNumberType shortPeerNumber)
{
    return shortPeerInfo.encode(fullId, shortPeerNumber);
}

} // namespace p2p
} // namespace nx
