#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>

#include <nx_ec/data/api_peer_data.h>
#include <nx_ec/data/api_tran_state_data.h>
#include "p2p_fwd.h"

namespace ec2 {
class QnAbstractTransaction;
}

namespace nx {
namespace p2p {

/** MiscData contains members that managed by P2pMessageBus. P2pConnection doesn't touch it */
class ConnectionContext: public QObject
{
public:
    ConnectionContext()
    {
        localPeersTimer.invalidate();
    }

    bool isRemotePeerSubscribedTo(const ec2::ApiPersistentIdData& peer) const;
    bool isRemotePeerSubscribedTo(const QnUuid& peer) const;
    bool isLocalPeerSubscribedTo(const ec2::ApiPersistentIdData& peer) const;
    bool updateSequence(const ec2::QnAbstractTransaction& tran);

    ec2::ApiPersistentIdData decode(PeerNumberType shortPeerNumber) const;
    PeerNumberType encode(const ec2::ApiPersistentIdData& fullId, PeerNumberType shortPeerNumber = kUnknownPeerNumnber);
public:
    // to local part
    QByteArray localPeersMessage; //< last sent peers message
    QElapsedTimer localPeersTimer; //< last sent peers time
    QVector<ec2::ApiPersistentIdData> localSubscription; //< local -> remote subscription
    bool isLocalStarted = false; //< we opened connection to remote peer
    QVector<PeerNumberType> awaitingNumbersToResolve;
    bool sendDataInProgress = false; //< Select from transaction log in progress

    // to remote part
    QByteArray remotePeersMessage; //< last received peers message
    QVector<PeerDistanceRecord> remotePeers;
    ec2::QnTranState remoteSubscription; //< remote -> local subscription
    bool remoteAddImplicitData = false; //< remote -> local subscription. Add implicit data to subscription (subscribeAll).
    bool recvDataInProgress = false;
    bool isRemoteStarted = false; //< remote peer has open logical connection to us
    PeerNumberInfo shortPeerInfo;

    std::function<void()> onConnectionClosedCallback;
};

} // namespace p2p
} // namespace nx
