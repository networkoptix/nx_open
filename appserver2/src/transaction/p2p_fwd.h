#pragma once

#include <QtCore/QtGlobal>
#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/serialization_format.h>

#include <nx_ec/data/api_peer_data.h>
#include <nx/fusion/fusion/fusion_fwd.h>
#include <utils/media/bitStream.h>

#ifdef THIS_BLOCK_IS_REQUIRED_TO_MAKE_FILE_BE_PROCESSED_BY_MOC_DO_NOT_DELETE
Q_OBJECT
#endif
QN_DECLARE_METAOBJECT_HEADER(ec2, MessageType, )

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
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(MessageType)

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

void serializeCompressPeerNumber(BitStreamWriter& writer, PeerNumberType peerNumber);
PeerNumberType deserializeCompressPeerNumber(BitStreamReader& reader);

void serializeCompressedSize(BitStreamWriter& writer, quint32 peerNumber);
quint32 deserializeCompressedSize(BitStreamReader& reader);

QString toString(MessageType value);

} // namespace ec2

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((ec2::MessageType), (metatype)(lexical))