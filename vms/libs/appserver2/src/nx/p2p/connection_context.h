// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>

#include <nx/vms/api/data/tran_state_data.h>

#include "p2p_fwd.h"

namespace ec2 { struct QnAbstractTransaction; }

namespace nx {
namespace p2p {

enum class UpdateSequenceResult
{
    ok,
    notSubscribed,
    alreadyKnown
};
QString toString(UpdateSequenceResult value);

/** MiscData contains members that managed by P2pMessageBus. P2pConnection doesn't touch it */
class ConnectionContext: public QObject
{
public:
    ConnectionContext()
    {
        localPeersTimer.invalidate();
    }

    bool isRemotePeerSubscribedTo(const vms::api::PersistentIdData& peer) const;
    bool isRemotePeerSubscribedTo(const nx::Uuid& peer) const;
    bool isLocalPeerSubscribedTo(const vms::api::PersistentIdData& peer) const;
    UpdateSequenceResult updateSequence(const ec2::QnAbstractTransaction& tran);

    vms::api::PersistentIdData decode(PeerNumberType shortPeerNumber) const;
    PeerNumberType encode(const vms::api::PersistentIdData& fullId,
        PeerNumberType shortPeerNumber = kUnknownPeerNumnber);
public:
    // to local part
    QByteArray localPeersMessage; //< last sent peers message
    QElapsedTimer localPeersTimer; //< last sent peers time
    QVector<vms::api::PersistentIdData> localSubscription; //< local -> remote subscription
    bool isLocalStarted = false; //< we opened connection to remote peer
    QVector<PeerNumberType> awaitingNumbersToResolve;
    bool sendDataInProgress = false; //< Select from transaction log in progress

    // to remote part
    QByteArray remotePeersMessage; //< last received peers message
    QVector<PeerDistanceRecord> remotePeers;
    vms::api::TranState remoteSubscription; //< remote -> local subscription
    bool isRemoteSubscribedToAll = false; //< remote -> local subscription. Add implicit data to subscription (subscribeAll).
    bool recvDataInProgress = false;
    bool isRemoteStarted = false; //< remote peer has open logical connection to us
    PeerNumberInfo shortPeerInfo;

    std::function<void()> onConnectionClosedCallback;
};

} // namespace p2p
} // namespace nx
