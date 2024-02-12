// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <QtCore/QMap>

#include <nx/vms/api/data/peer_data.h>

namespace nx::p2p {

NX_REFLECTION_ENUM_CLASS(MessageType,
    unknown,
    start,
    stop,
    resolvePeerNumberRequest,
    resolvePeerNumberResponse,
    alivePeers,
    subscribeForDataUpdates, //< Subscribe for specified ID numbers
    pushTransactionData, //< transaction data
    pushTransactionList, //< for UbJson format only. transaction list
    pushImpersistentBroadcastTransaction, //< transportHeader + transaction data
    pushImpersistentUnicastTransaction, //< transportHeader + transaction data

    /**
     * Subscribe for all data updates. This request contains current peer state.
     * Foreign peer MUST send all its data above described state. Unlike 'subscribeForDataUpdates'
     * this request sends all other data which are not mentioned in request.
     */
    subscribeAll,
    counter
)

using PeerNumberType = quint16;
const static PeerNumberType kUnknownPeerNumnber = 0xffff;

struct PeerNumberInfo
{
    vms::api::PersistentIdData decode(PeerNumberType number) const;
    PeerNumberType encode(const vms::api::PersistentIdData& peer,
        PeerNumberType shortNumber = kUnknownPeerNumnber);
private:
    QMap<vms::api::PersistentIdData, PeerNumberType> m_fullIdToShortId;
    QMap<PeerNumberType, vms::api::PersistentIdData> m_shortIdToFullId;
};

struct SubscribeRecord
{
    PeerNumberType peer = 0;
    qint32 sequence = 0;
};

struct PeerDistanceRecord
{
    static constexpr int kMaxRecordSize = 9; // 2 bytes number + 4 bytes distance + online flag + 2 bytes via

    PeerNumberType peerNumber = 0;
    qint32 distance = 0; //< Distance to the peer.
    PeerNumberType firstVia = kUnknownPeerNumnber; //< First via peer in route if distance > 1.
};

struct PeerNumberResponseRecord: vms::api::PersistentIdData
{
    static constexpr int kRecordSize = 16 * 2 + 2; //< two guid + uncompressed PeerNumber per record

    PeerNumberResponseRecord() = default;
    PeerNumberResponseRecord(PeerNumberType peerNumber, const vms::api::PersistentIdData& id):
        vms::api::PersistentIdData(id),
        peerNumber(peerNumber)
    {
    }

    PeerNumberType peerNumber = 0;
};

struct BidirectionRoutingInfo;

struct TransportHeader
{
    std::set<nx::Uuid> via;
    std::vector<nx::Uuid> dstPeers;
};
#define TransportHeader_Fields (via)

static const qint32 kMaxDistance = std::numeric_limits<qint32>::max();
static const qint32 kMaxOnlineDistance = 16384;
extern const char* const kP2pProtoName;

} // namespace nx::p2p
