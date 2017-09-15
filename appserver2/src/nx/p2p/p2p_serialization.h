#pragma  once

#include <utils/media/bitStream.h>
#include <nx_ec/data/api_peer_data.h>
#include "p2p_fwd.h"

namespace nx {
namespace p2p {

void serializeCompressPeerNumber(BitStreamWriter& writer, PeerNumberType peerNumber);
PeerNumberType deserializeCompressPeerNumber(BitStreamReader& reader);

void serializeCompressedSize(BitStreamWriter& writer, quint32 size);
quint32 deserializeCompressedSize(BitStreamReader& reader);

QByteArray serializePeersMessage(const QVector<PeerDistanceRecord>& records, int reservedSpaceAtFront = 1);
QVector<PeerDistanceRecord> deserializePeersMessage(const QByteArray& data, bool* success);

QByteArray serializeCompressedPeers(const QVector<PeerNumberType>& peers, int reservedSpaceAtFront = 1);
QVector<PeerNumberType> deserializeCompressedPeers(const QByteArray& data, bool* success);

QByteArray serializeSubscribeRequest(const QVector<SubscribeRecord>& request, int reservedSpaceAtFront = 1);
QVector<SubscribeRecord> deserializeSubscribeRequest(const QByteArray& data, bool* success);

QByteArray serializeResolvePeerNumberResponse(const QVector<PeerNumberResponseRecord>& peers, int reservedSpaceAtFront = 1);
const QVector<PeerNumberResponseRecord> deserializeResolvePeerNumberResponse(const QByteArray& data, bool* success);

QByteArray serializeTransactionList(const QList<QByteArray>& tranList, int reservedSpaceAtFront = 1);
QList<QByteArray> deserializeTransactionList(const QByteArray& tranList, bool* success);

QByteArray serializeTransportHeader(const TransportHeader& records);
TransportHeader deserializeTransportHeader(const QByteArray& data, int* bytesRead);

QByteArray serializeSubscribeAllRequest(const ec2::QnTranState& request, int reservedSpaceAtFront = 1);
ec2::QnTranState deserializeSubscribeAllRequest(const QByteArray& data, bool* success);

QString toString(MessageType value);

} // namespace p2p
} // namespace nx
