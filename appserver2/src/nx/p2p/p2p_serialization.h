#pragma  once

#include <utils/media/bitStream.h>
#include <nx_ec/data/api_peer_data.h>
#include "p2p_fwd.h"

namespace nx {
namespace p2p {

void serializeCompressPeerNumber(BitStreamWriter& writer, PeerNumberType peerNumber);
PeerNumberType deserializeCompressPeerNumber(BitStreamReader& reader);

void serializeCompressedSize(BitStreamWriter& writer, quint32 peerNumber);
quint32 deserializeCompressedSize(BitStreamReader& reader);

QByteArray serializePeersMessage(
    const BidirectionRoutingInfo* peers,
    PeerNumberInfo& shortPeerInfo);
bool deserializePeersMessage(
    const ec2::ApiPersistentIdData& remotePeer,
    int remotePeerDistance,
    const PeerNumberInfo& shortPeerInfo,
    const QByteArray& data,
    const qint64 timeMs,
    BidirectionRoutingInfo* outPeers);

QByteArray serializeCompressedPeers(const QVector<PeerNumberType>& peers, int reservedSpaceAtFront = 1);
QVector<PeerNumberType> deserializeCompressedPeers(const QByteArray& data, bool* success);

QByteArray serializeSubscribeRequest(const QVector<SubscribeRecord>& request);
QVector<SubscribeRecord> deserializeSubscribeRequest(const QByteArray& data, bool* success);

QByteArray serializeResolvePeerNumberResponse(
    const QVector<PeerNumberType>& peers,
    const PeerNumberInfo& peerNumberInfo,
    int reservedSpaceAtFront = 1);

QByteArray serializeTransactionList(const QList<QByteArray>& tranList, int reservedSpaceAtFront = 1);
QVector<QByteArray> deserializeTransactionList(const QByteArray& tranList, bool* success);

QString toString(MessageType value);

} // namespace p2p
} // namespace nx
