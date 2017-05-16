#include "p2p_serialization.h"

#include <array>

#include <QtCore/QBuffer>

#include <nx/fusion/serialization/lexical.h>
#include <nx/fusion/serialization/lexical_enum.h>
#include <utils/math/math.h>
#include "routing_helpers.h"
#include <utils/media/nalUnits.h>

namespace {
    static const int kByteArrayAlignFactor = sizeof(unsigned);
    static const int kPeerRecordSize = 6;
    static const int kResolvePeerResponseRecordSize = 16 * 2 + 2; //< two guid + uncompressed PeerNumber per record
}

namespace nx {
namespace p2p {

template <std::size_t N>
void serializeCompressedValue(BitStreamWriter& writer, quint32 value, const std::array<int, N>& bitsGroups)
{
    for (int i = 0; i < bitsGroups.size(); ++i)
    {
        writer.putBits(bitsGroups[i], value);
        if (i == bitsGroups.size() - 1)
            break; //< 16 bits written

        value >>= bitsGroups[i];
        if (value == 0)
        {
            writer.putBit(0); //< end of number
            break;
        }
        writer.putBit(1); //< number continuation
    }
}

template <std::size_t N>
quint32 deserializeCompressedValue(BitStreamReader& reader, const std::array<int, N>& bitsGroups)
{
    quint32 value = 0;
    int shift = 0;
    for (int i = 0; i < bitsGroups.size(); ++i)
    {
        value += (reader.getBits(bitsGroups[i]) << shift);
        if (i == bitsGroups.size() - 1 || reader.getBit() == 0)
            break;
        shift += bitsGroups[i];
    }
    return value;
}

const static std::array<int, 3> peerNumberbitsGroups = { 7, 3, 4 };

void serializeCompressPeerNumber(BitStreamWriter& writer, PeerNumberType peerNumber)
{
    serializeCompressedValue(writer, peerNumber, peerNumberbitsGroups);
}

PeerNumberType deserializeCompressPeerNumber(BitStreamReader& reader)
{
    return deserializeCompressedValue(reader, peerNumberbitsGroups);
}

const static std::array<int, 4> compressedSizebitsGroups = { 7, 7, 7, 8 };

void serializeCompressedSize(BitStreamWriter& writer, quint32 peerNumber)
{
    serializeCompressedValue(writer, peerNumber, compressedSizebitsGroups);
}

quint32 deserializeCompressedSize(BitStreamReader& reader)
{
    return deserializeCompressedValue(reader, compressedSizebitsGroups);
}

QString toString(MessageType value)
{
    return QnLexical::serialized(value);
}

QByteArray serializePeersMessage(
    const BidirectionRoutingInfo* peers,
    PeerNumberInfo& shortPeerInfo)
{
    QByteArray result;
    result.resize(qPower2Ceil(
        unsigned(peers->allPeerDistances.size() * kPeerRecordSize + 1),
        4));
    BitStreamWriter writer;
    writer.setBuffer((quint8*)result.data(), result.size());
    try
    {
        writer.putBits(8, (int)MessageType::alivePeers);

        // serialize online peers
        for (auto itr = peers->allPeerDistances.cbegin(); itr != peers->allPeerDistances.cend(); ++itr)
        {
            const auto& peer = itr.value();
            qint32 minDistance = peer.minDistance();
            if (minDistance == kMaxDistance)
                continue;
            const qint16 peerNumber = shortPeerInfo.encode(itr.key());
            serializeCompressPeerNumber(writer, peerNumber);
            const bool isOnline = minDistance < kMaxOnlineDistance;
            writer.putBit(isOnline);
            if (isOnline)
                NALUnit::writeUEGolombCode(writer, minDistance); //< distance
            else
                writer.putBits(32, minDistance); //< distance
        }

        writer.flushBits(true);
        result.truncate(writer.getBytesCount());
        return result;
    }
    catch (...)
    {
        return QByteArray();
    }
}

bool deserializePeersMessage(
    const ec2::ApiPersistentIdData& remotePeer,
    int remotePeerDistance,
    const PeerNumberInfo& shortPeerInfo,
    const QByteArray& data,
    const qint64 timeMs,
    BidirectionRoutingInfo* outPeers)
{
    outPeers->removePeer(remotePeer);

    BitStreamReader reader((const quint8*)data.data(), data.size());
    try
    {
        while (reader.bitsLeft() >= 8)
        {
            PeerNumberType peerNumber = deserializeCompressPeerNumber(reader);
            ApiPersistentIdData peerId = shortPeerInfo.decode(peerNumber);
            bool isOnline = reader.getBit();
            qint32 receivedDistance = 0;
            if (isOnline)
                receivedDistance = NALUnit::extractUEGolombCode(reader); // todo: move function to another place
            else
                receivedDistance = reader.getBits(32);

            outPeers->addRecord(remotePeer, peerId, nx::p2p::RoutingRecord(receivedDistance + remotePeerDistance, timeMs));
        }
    }
    catch (...)
    {
        return false;
    }
    return true;
}

QByteArray serializeCompressedPeers(const QVector<PeerNumberType>& peers, int reservedSpaceAtFront)
{
    QByteArray result;
    result.resize(qPower2Ceil(unsigned(peers.size() * 2 + 1), kByteArrayAlignFactor));
    BitStreamWriter writer;
    writer.setBuffer((quint8*)result.data(), result.size());
    writer.putBits(reservedSpaceAtFront * 8, 0);
    for (const auto& peer : peers)
        serializeCompressPeerNumber(writer, peer);
    writer.flushBits(true);
    result.truncate(writer.getBytesCount());
    return result;
}

QVector<PeerNumberType> deserializeCompressedPeers(const QByteArray& data, bool* success)
{
    QVector<PeerNumberType> result;
    *success = true;
    if (data.isEmpty())
        return result;
    BitStreamReader reader((const quint8*)data.data(), data.size());
    try
    {
        while (reader.bitsLeft() >= 8)
            result.push_back(deserializeCompressPeerNumber(reader));
    }
    catch (...)
    {
        *success = false;
    }
    return result;
}

QByteArray serializeSubscribeRequest(const QVector<SubscribeRecord>& request)
{
    QByteArray result;
    result.resize(qPower2Ceil(unsigned(request.size() * kPeerRecordSize + 1), kByteArrayAlignFactor));
    BitStreamWriter writer;
    writer.setBuffer((quint8*) result.data(), result.size());
    writer.putBits(8, (int)MessageType::subscribeForDataUpdates);
    for (const auto& record: request)
    {
        writer.putBits(16, record.peer);
        writer.putBits(32, record.sequence);
    }
    writer.flushBits(true);
    result.truncate(writer.getBytesCount());
    return result;
}

QVector<SubscribeRecord> deserializeSubscribeRequest(const QByteArray& data, bool* success)
{
    QVector<SubscribeRecord> result;
    if (data.isEmpty())
        return result;
    BitStreamReader reader((quint8*)data.data(), data.size());
    try {
        while (reader.bitsLeft() > 0)
        {
            PeerNumberType peer = reader.getBits(16);
            qint32 sequence = reader.getBits(32);
            result.push_back(SubscribeRecord(peer, sequence));
        }
        *success = true;
    }
    catch (...)
    {
        *success = false;
    }
    return result;
}

QByteArray serializeTransactionList(const QList<QByteArray>& tranList, int reservedSpaceAtFront)
{
    static const int kMaxHeaderSize = 4;
    unsigned expectedSize = 1;
    for (const auto& tran : tranList)
        expectedSize += kMaxHeaderSize + tran.size();
    QByteArray message;
    message.resize(qPower2Ceil(expectedSize, 4));
    BitStreamWriter writer;
    writer.setBuffer((quint8*)message.data(), message.size());
    writer.putBits(8 * reservedSpaceAtFront, 0);
    for (const auto& tran : tranList)
    {
        serializeCompressedSize(writer, tran.size());
        writer.putBytes((quint8*)tran.data(), tran.size());
    }
    writer.flushBits(true);
    message.truncate(writer.getBytesCount());
    return message;
}

QVector<QByteArray> deserializeTransactionList(const QByteArray& tranList, bool* success)
{
    QVector<QByteArray> result;
    BitStreamReader reader((const quint8*) tranList.data(), tranList.size());
    try
    {
        while (reader.bitsLeft() > 0)
        {
            quint32 size = deserializeCompressedSize(reader);
            int offset = reader.getBitsCount() / 8;
            result.push_back(QByteArray::fromRawData(tranList.data() + offset, size));
            reader.skipBytes(size);
        }
        *success = true;
    }
    catch (...)
    {
        *success = false;
    }
    return result;
}

QByteArray serializeResolvePeerNumberResponse(
    const QVector<PeerNumberType>& peers,
    const PeerNumberInfo& peerNumberInfo,
    int reservedSpaceAtFront)
{
    QByteArray result;
    result.reserve(peers.size() * kResolvePeerResponseRecordSize + 1);
    {
        QBuffer buffer(&result);
        buffer.open(QIODevice::WriteOnly);
        QDataStream out(&buffer);

        for (int i = 0; i < reservedSpaceAtFront; ++i)
            out << (quint8) 0;
        for (const auto& peer : peers)
        {
            out << peer;
            ApiPersistentIdData fullId = peerNumberInfo.decode(peer);
            out.writeRawData(fullId.id.toRfc4122().data(), 16);
            out.writeRawData(fullId.persistentId.toRfc4122().data(), 16);
        }
    }
    return result;
}

} // namespace p2p
} // namespace nx
