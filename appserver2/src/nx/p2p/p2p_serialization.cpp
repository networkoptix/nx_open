#include "p2p_serialization.h"

#include <array>

#include <QtCore/QBuffer>

#include <nx/fusion/serialization/lexical.h>
#include <nx/fusion/serialization/lexical_enum.h>
#include <utils/math/math.h>
#include "routing_helpers.h"
#include <utils/media/nalUnits.h>
#include <nx_ec/data/api_tran_state_data.h>

namespace {
    static const int kByteArrayAlignFactor = sizeof(unsigned);
    static const int kGuidSize = 16;
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

void serializeCompressedSize(BitStreamWriter& writer, quint32 size)
{
    serializeCompressedValue(writer, size, compressedSizebitsGroups);
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
    const QVector<PeerDistanceRecord>& records,
    int reservedSpaceAtFront)
{
    QByteArray result;
    result.resize(qPower2Ceil(
        unsigned(records.size() * PeerDistanceRecord::kMaxRecordSize + reservedSpaceAtFront),
        kByteArrayAlignFactor));
    BitStreamWriter writer;
    writer.setBuffer((quint8*) result.data(), result.size());
    try
    {
        writer.putBits(reservedSpaceAtFront * 8, 0);

        for (const auto& record: records)
        {
            serializeCompressPeerNumber(writer, record.peerNumber);
            const bool isOnline = record.distance < kMaxOnlineDistance;
            writer.putBit(isOnline);
            if (isOnline)
                NALUnit::writeUEGolombCode(writer, record.distance);
            else
                writer.putBits(32, record.distance);
        }
        writer.flushBits(true);
        result.truncate(writer.getBytesCount());
        return result;
    }
    catch (const BitStreamException&)
    {
        return QByteArray();
    }
}

QVector<PeerDistanceRecord> deserializePeersMessage(const QByteArray& data, bool* success)
{
    QVector<PeerDistanceRecord> result;
    BitStreamReader reader((const quint8*)data.data(), data.size());
    try
    {
        *success = true;
        while (reader.bitsLeft() >= 8)
        {
            PeerNumberType peerNumber = deserializeCompressPeerNumber(reader);
            bool isOnline = reader.getBit();
            qint32 distance = 0;
            if (isOnline)
                distance = NALUnit::extractUEGolombCode(reader); // todo: move function to another place
            else
                distance = reader.getBits(32);
            result.push_back(PeerDistanceRecord(peerNumber, distance));
        }
    }
    catch (const BitStreamException&)
    {
        *success = false;
    }
    return result;
}

QByteArray serializeCompressedPeers(const QVector<PeerNumberType>& peers, int reservedSpaceAtFront)
{
    QByteArray result;
    result.resize(qPower2Ceil(unsigned(peers.size() * 2 + reservedSpaceAtFront), kByteArrayAlignFactor));
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
    catch (const BitStreamException&)
    {
        *success = false;
    }
    return result;
}

QByteArray serializeSubscribeRequest(const QVector<SubscribeRecord>& request, int reservedSpaceAtFront)
{
    QByteArray result;
    result.resize(qPower2Ceil(unsigned(request.size() * PeerDistanceRecord::kMaxRecordSize +
        reservedSpaceAtFront), kByteArrayAlignFactor));
    BitStreamWriter writer;
    writer.setBuffer((quint8*) result.data(), result.size());
    writer.putBits(reservedSpaceAtFront * 8, 0);
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
    catch (const BitStreamException&)
    {
        *success = false;
    }
    return result;
}

QByteArray serializeSubscribeAllRequest(const ec2::QnTranState& request, int reservedSpaceAtFront)
{
    QByteArray result;
    {
        QBuffer buffer(&result);
        buffer.open(QIODevice::WriteOnly);
        QDataStream out(&buffer);

        for (int i = 0; i < reservedSpaceAtFront; ++i)
            out << (quint8) 0;

        for (auto itr = request.values.begin(); itr != request.values.end(); ++itr)
        {
            const ApiPersistentIdData& peer = itr.key();
            qint32 sequence = itr.value();

            out.writeRawData(peer.id.toRfc4122().data(), kGuidSize);
            out.writeRawData(peer.persistentId.toRfc4122().data(), kGuidSize);
            out << sequence;
        }
    }
    return result;
}

ec2::QnTranState deserializeSubscribeAllRequest(const QByteArray& _response, bool* success)
{
    QByteArray response(_response);
    ec2::QnTranState result;
    *success = false;

    QBuffer buffer(&response);
    buffer.open(QIODevice::ReadOnly);
    QDataStream in(&buffer);
    QByteArray tmpBuffer;
    tmpBuffer.resize(kGuidSize);
    ApiPersistentIdData fullId;
    qint32 sequence;
    while (!in.atEnd())
    {
        if (in.readRawData(tmpBuffer.data(), tmpBuffer.size()) != kGuidSize)
            return result; //< Error.
        fullId.id = QnUuid::fromRfc4122(tmpBuffer);
        if (in.readRawData(tmpBuffer.data(), tmpBuffer.size()) != kGuidSize)
            return result; //< Error.
        fullId.persistentId = QnUuid::fromRfc4122(tmpBuffer);
        in >> sequence;

        result.values.insert(fullId, sequence);
    }
    *success = true;
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

QList<QByteArray> deserializeTransactionList(const QByteArray& tranList, bool* success)
{
    QList<QByteArray> result;
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
    catch (const BitStreamException&)
    {
        *success = false;
    }
    return result;
}

QByteArray serializeResolvePeerNumberResponse(
    const QVector<PeerNumberResponseRecord>& peers,
    int reservedSpaceAtFront)
{
    QByteArray result;
    result.reserve(peers.size() * PeerNumberResponseRecord::kRecordSize + reservedSpaceAtFront);
    {
        QBuffer buffer(&result);
        buffer.open(QIODevice::WriteOnly);
        QDataStream out(&buffer);

        for (int i = 0; i < reservedSpaceAtFront; ++i)
            out << (quint8) 0;
        for (const auto& peer: peers)
        {
            out << peer.peerNumber;
            out.writeRawData(peer.id.toRfc4122().data(), kGuidSize);
            out.writeRawData(peer.persistentId.toRfc4122().data(), kGuidSize);
        }
    }
    return result;
}

const QVector<PeerNumberResponseRecord> deserializeResolvePeerNumberResponse(const QByteArray& _response, bool* success)
{
    QByteArray response(_response);
    QVector<PeerNumberResponseRecord> result;

    *success = false;
    if (response.size() % PeerNumberResponseRecord::kRecordSize != 0)
        return result;

    QBuffer buffer(&response);
    buffer.open(QIODevice::ReadOnly);
    QDataStream in(&buffer);
    QByteArray tmpBuffer;
    tmpBuffer.resize(kGuidSize);
    PeerNumberType shortPeerNumber;
    ApiPersistentIdData fullId;
    while (!in.atEnd())
    {
        in >> shortPeerNumber;
        if (in.readRawData(tmpBuffer.data(), tmpBuffer.size()) != kGuidSize)
            return result;
        fullId.id = QnUuid::fromRfc4122(tmpBuffer);
        if (in.readRawData(tmpBuffer.data(), tmpBuffer.size()) != kGuidSize)
            return result;
        fullId.persistentId = QnUuid::fromRfc4122(tmpBuffer);

        result.push_back(PeerNumberResponseRecord(shortPeerNumber, fullId));
    }
    *success = true;
    return result;
}

QByteArray serializeTransportHeader(const TransportHeader& header)
{
    QByteArray result;
    QBuffer buffer(&result);
    buffer.open(QIODevice::WriteOnly);
    QDataStream out(&buffer);

    out << (quint32) header.via.size();
    for (const auto& value: header.via)
        out.writeRawData(value.toRfc4122().data(), kGuidSize);

    out << (quint32) header.dstPeers.size();
    for (const auto& value : header.dstPeers)
        out.writeRawData(value.toRfc4122().data(), kGuidSize);

    return result;
}

TransportHeader deserializeTransportHeader(const QByteArray& response, int* bytesRead)
{
    QByteArray responseCopy(response);
    TransportHeader result;

    *bytesRead = -1;

    QBuffer buffer(&responseCopy);
    buffer.open(QIODevice::ReadOnly);
    QDataStream in(&buffer);
    QByteArray tmpBuffer;
    tmpBuffer.resize(kGuidSize);
    ApiPersistentIdData fullId;
    quint32 size;

    if (in.atEnd())
        return result; //< error
    in >> size;
    while (!in.atEnd() && size > 0)
    {
        if (in.readRawData(tmpBuffer.data(), tmpBuffer.size()) != kGuidSize)
            return result; //< Error
        result.via.insert(QnUuid::fromRfc4122(tmpBuffer));
        --size;
    }

    if (in.atEnd())
        return result; //< error
    in >> size;
    while (!in.atEnd() && size > 0)
    {
        if (in.readRawData(tmpBuffer.data(), tmpBuffer.size()) != kGuidSize)
            return result; //< Error
        result.dstPeers.push_back(QnUuid::fromRfc4122(tmpBuffer));
        --size;
    }

    *bytesRead = buffer.pos();
    return result;
}

} // namespace p2p
} // namespace nx
