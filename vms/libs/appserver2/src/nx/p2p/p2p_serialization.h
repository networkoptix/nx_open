#pragma  once

#include "p2p_fwd.h"

#include <utils/media/bitStream.h>

#include <nx/network/http/http_types.h>
#include <nx/vms/api/data_fwd.h>
#include <nx/vms/api/data/tran_state_data.h>

namespace nx {
namespace p2p {

void serializeCompressPeerNumber(BitStreamWriter& writer, PeerNumberType peerNumber);
PeerNumberType deserializeCompressPeerNumber(BitStreamReader& reader);

void serializeCompressedSize(BitStreamWriter& writer, quint32 size);
quint32 deserializeCompressedSize(BitStreamReader& reader);

QByteArray serializePeersMessage(const std::vector<PeerDistanceRecord>& records, int reservedSpaceAtFront = 1);
std::vector<PeerDistanceRecord> deserializePeersMessage(const QByteArray& data, bool* success);

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

QByteArray serializeSubscribeAllRequest(const vms::api::TranState& request, int reservedSpaceAtFront = 1);
vms::api::TranState deserializeSubscribeAllRequest(const QByteArray& data, bool* success);

vms::api::PeerDataEx deserializePeerData(
    const network::http::HttpHeaders& headers, Qn::SerializationFormat dataFormat);
vms::api::PeerDataEx deserializePeerData(const network::http::Request& request);
void serializePeerData(network::http::HttpHeaders& headers,
    vms::api::PeerDataEx peer, Qn::SerializationFormat dataFormat);
void serializePeerData(network::http::Response& response,
    vms::api::PeerDataEx peer, Qn::SerializationFormat dataFormat);

QString toString(MessageType value);

} // namespace p2p
} // namespace nx
