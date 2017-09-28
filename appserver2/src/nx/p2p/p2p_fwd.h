#pragma once

#include <set>
#include <QtCore/QtGlobal>
#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/serialization_format.h>

#include <nx_ec/data/api_peer_data.h>
#include <nx/fusion/fusion/fusion_fwd.h>

namespace nx {

#ifdef THIS_BLOCK_IS_REQUIRED_TO_MAKE_FILE_BE_PROCESSED_BY_MOC_DO_NOT_DELETE
Q_OBJECT
#endif
QN_DECLARE_METAOBJECT_HEADER(p2p, MessageType, )

enum class MessageType
{
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
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(MessageType)

using PeerNumberType = quint16;
const static PeerNumberType kUnknownPeerNumnber = 0xffff;

struct PeerNumberInfo
{
    ec2::ApiPersistentIdData decode(PeerNumberType number) const;
    PeerNumberType encode(const ec2::ApiPersistentIdData& peer, PeerNumberType shortNumber = kUnknownPeerNumnber);
private:
    QMap<ec2::ApiPersistentIdData, PeerNumberType> m_fullIdToShortId;
    QMap<PeerNumberType, ec2::ApiPersistentIdData> m_shortIdToFullId;
};

struct SubscribeRecord
{
    SubscribeRecord() {}
    SubscribeRecord(PeerNumberType peer, qint32 sequence): peer(peer), sequence(sequence) {}

    PeerNumberType peer = 0;
    qint32 sequence = 0;
};

struct PeerDistanceRecord
{
    PeerDistanceRecord() {}
    PeerDistanceRecord(PeerNumberType peerNumber, qint32 distance):
        peerNumber(peerNumber),
        distance(distance)
    {
    }
    static const int kMaxRecordSize = 7; // 2 bytes number + 4 bytes distance + online flag

    PeerNumberType peerNumber = 0;
    qint32 distance = 0;
};

struct PeerNumberResponseRecord: public ec2::ApiPersistentIdData
{
    static const int kRecordSize = 16 * 2 + 2; //< two guid + uncompressed PeerNumber per record

    PeerNumberResponseRecord() {}
    PeerNumberResponseRecord(PeerNumberType peerNumber, const ec2::ApiPersistentIdData& id):
        ec2::ApiPersistentIdData(id),
        peerNumber(peerNumber)
    {
    }

    PeerNumberType peerNumber = 0;
};

struct BidirectionRoutingInfo;

#if 0
struct UnicastTransactionRecord
{
    UnicastTransactionRecord() {}
    UnicastTransactionRecord(const QnUuid& dstPeer, int ttl):
        dstPeer(dstPeer),
        ttl(ttl)
    {
    }
    static const int kRecordSize = 16 + 1;
    QnUuid dstPeer;
    quint8 ttl = 0;
};
using UnicastTransactionRecords = std::vector<UnicastTransactionRecord>;
#endif

struct TransportHeader
{
    std::set<QnUuid> via;
    std::vector<QnUuid> dstPeers;
};
#define TransportHeader_Fields (via)

static const qint32 kMaxDistance = std::numeric_limits<qint32>::max();
static const qint32 kMaxOnlineDistance = 16384;
extern const char* const kP2pProtoName;

} // namespace p2p
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::p2p::MessageType), (metatype)(lexical))
//QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::p2p::TransportHeader), (ubjson))
