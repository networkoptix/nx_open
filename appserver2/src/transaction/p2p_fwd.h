#pragma once
#include <nx_ec/data/api_peer_data.h>
#include <nx/fusion/fusion/fusion_fwd.h>
#include <utils/media/bitStream.h>

namespace ec2 {

enum class MessageType
{
    start,
    stop,
    resolvePeerNumberRequest,
    resolvePeerNumberResponse,
    alivePeers,
    subscribeForDataUpdates,
    pushTransactionData,
    pushTransactionList, //< for UbJson format only

    counter
};

using PeerNumberType = quint16;
const static PeerNumberType kUnknownPeerNumnber = 0xffff;

struct PeerNumberInfo
{
    ApiPersistentIdData decode(PeerNumberType number) const;
    PeerNumberType encode(const ApiPersistentIdData& peer, PeerNumberType shortNumber = kUnknownPeerNumnber);
private:
    QMap<ApiPersistentIdData, PeerNumberType> m_fullIdToShortId;
    QMap<PeerNumberType, ApiPersistentIdData> m_shortIdToFullId;
};

struct SubscribeRecord
{
    SubscribeRecord() {}
    SubscribeRecord(PeerNumberType peer, qint32 sequence): peer(peer), sequence(sequence) {}

    PeerNumberType peer = 0;
    qint32 sequence = 0;
};

static const qint32 kMaxDistance = std::numeric_limits<qint32>::max();
static const qint32 kMaxOnlineDistance = 16384;
static const char* kP2pProtoName = "p2p";

Q_DECLARE_METATYPE(MessageType)

void serializeCompressPeerNumber(BitStreamWriter& writer, PeerNumberType peerNumber);
PeerNumberType deserializeCompressPeerNumber(BitStreamReader& reader);

void serializeCompressedSize(BitStreamWriter& writer, quint32 peerNumber);
quint32 deserializeCompressedSize(BitStreamReader& reader);

} // namespace ec2
