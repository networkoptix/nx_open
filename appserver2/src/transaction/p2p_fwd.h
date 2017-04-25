#pragma once
#include <nx_ec/data/api_peer_data.h>

namespace ec2 {

enum class MessageType
{
    resolvePeerNumberRequest,
    resolvePeerNumberResponse,
    alivePeers,
    subscribeForDataUpdates,
    pushTransactionData,
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
static const char* kP2pProtoName = "p2p";

Q_DECLARE_METATYPE(MessageType)

} // namespace ec2
