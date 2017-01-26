#include "mdns_packet.h"

#ifdef ENABLE_MDNS

#include <QtCore/QDataStream>
#include <QtCore/QIODevice>


void QnMdnsPacket::toDatagram(QByteArray& datagram)
{
    QDataStream stream(&datagram, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream
        << transactionId
        << flags
        << questionCount
        << answerRRCount
        << authorityRRCount
        << additionalRRCount;

    for (const Query& query: queries)
    {
        stream.writeRawData(query.queryName.data(), query.queryName.size());
        stream << (quint8)0;
        stream << query.queryType;
        stream.writeRawData(query.queryData.data(), query.queryData.size());
    }
}

bool QnMdnsPacket::fromDatagram(const QByteArray& message)
{
    const auto bufferStart = message.constData();
    const auto bufferSize = (size_t)message.size();

    const size_t kHeaderLength =
        sizeof(decltype(transactionId)) +
        sizeof(decltype(flags)) +
        sizeof(decltype(questionCount)) +
        sizeof(decltype(answerRRCount)) +
        sizeof(decltype(authorityRRCount)) +
        sizeof(decltype(additionalRRCount));

    QDataStream stream(
        const_cast<QByteArray*>(&message),
        QIODevice::ReadOnly);

    stream.setByteOrder(QDataStream::BigEndian);
    stream
        >> transactionId
        >> flags
        >> questionCount
        >> answerRRCount
        >> authorityRRCount
        >> additionalRRCount;

    size_t currentOffset = kHeaderLength;
    for (int i = 0; i < questionCount; i++)
    {
        Query query;
        query.queryName.clear();

        auto nameLength = parseName(message, currentOffset, query.queryName);

        if(nameLength == -1)
            return false;

        currentOffset += nameLength;

        size_t sizeOfTypeAndClass =
            sizeof(query.queryType) +
            sizeof(query.queryClass);

        if (currentOffset + sizeOfTypeAndClass > bufferSize )
            return false;

        QByteArray queryTypeAndClassBuf(
            bufferStart + currentOffset,
            sizeOfTypeAndClass);
        QDataStream queryTypeAndClassStream(&queryTypeAndClassBuf, QIODevice::ReadOnly);
        queryTypeAndClassStream.setByteOrder(QDataStream::BigEndian);
        queryTypeAndClassStream
            >> query.queryType
            >> query.queryClass;

        currentOffset += sizeOfTypeAndClass;

        queries.push_back(query);
    }

    size_t recordCount =
        answerRRCount +
        authorityRRCount +
        additionalRRCount;

    for (size_t i = 0; i < recordCount; i++)
    {
        ResourceRecord record;

        auto nameLength = parseName(message, currentOffset, record.recordName);

        if(nameLength == -1)
            return false;

        currentOffset += nameLength;
        size_t fixedSizeFieldSize =
            sizeof(record.recordType) +
            sizeof(record.recordClass) +
            sizeof(record.ttl) +
            sizeof(record.dataLength);

        if (currentOffset + fixedSizeFieldSize > bufferSize)
            return false;

        QByteArray recordTypeAndClassBuf(bufferStart + currentOffset, fixedSizeFieldSize);
        QDataStream recordTypeAndClassStream(&recordTypeAndClassBuf, QIODevice::ReadOnly);

        recordTypeAndClassStream.setByteOrder(QDataStream::BigEndian);
        recordTypeAndClassStream
            >> record.recordType
            >> record.recordClass
            >> record.ttl
            >> record.dataLength;

        currentOffset += fixedSizeFieldSize;

        if(currentOffset + record.dataLength > bufferSize)
            return false;

        record.data.append(bufferStart + currentOffset, record.dataLength);

        if (i < answerRRCount)
            answerRRs.push_back(record);
        else if (i < answerRRCount + authorityRRCount)
            authorityRRs.push_back(record);
        else
            additionalRRs.push_back(record);

        currentOffset += record.dataLength;
    }

    return true;
}

int QnMdnsPacket::parseName(
    const QByteArray &message,
    int nameOffset,
    QByteArray& name)
{
    int nameSize = 0;

    parseNameInternal(message, nameOffset, name, nameSize, false);

    return nameSize;
}

void QnMdnsPacket::parseNameInternal(
    const QByteArray &message,
    int nameOffset,
    QByteArray& name,
    int& nameSize,
    bool readByPointer)
{
    size_t bufferSize = message.size();
    quint8* messageStart = (quint8*)message.constData();
    quint8* nameStart = messageStart + nameOffset;

    //starts with 11 bits
    bool isPointer = *nameStart >= 0xc0;

    //starts with 00 bits
    bool isLabel = *nameStart <= 0x3f;

    if (!isPointer && !isLabel)
    {
        nameSize = -1;
        return;
    }

    if (isLabel)
    {
        auto labelLength = *nameStart;
        if (!readByPointer)
            nameSize += labelLength + 1;

        //termination octet
        if (labelLength == 0)
            return;

        nameStart++;

        if (nameOffset + nameSize > (int)bufferSize)
        {
            nameSize = -1;
            return;
        }

        name.append((const char*)nameStart, labelLength);
        name.append('.');

        parseNameInternal(
            message,
            nameOffset + labelLength + 1,
            name,
            nameSize,
            readByPointer);
    }
    else
    {

        quint16 pointer = 0;
        const quint16 mask = 0x3fff;
        QByteArray pointerBuf((char*)(messageStart + nameOffset), sizeof(quint16));
        QDataStream pointerStream(&pointerBuf, QIODevice::ReadOnly);

        pointerStream.setByteOrder(QDataStream::BigEndian);
        pointerStream >> pointer;

        quint16 offset = pointer & mask;

        if (offset > bufferSize)
        {
            nameSize = -1;
            return;
        }

        if (!readByPointer)
            nameSize +=2;

        parseNameInternal(
            message,
            offset,
            name,
            nameSize,
            true);
    }

    return;

}

void QnMdnsPacket::addQuery()
{
    queries.append(Query());
    questionCount++;
}


void QnMdnsSrvData::decode(const QByteArray& raw)
{
    QDataStream stream(const_cast<QByteArray*>(&raw), QIODevice::ReadOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    stream
        >> priority
        >> weight
        >> port;

    auto position =
        raw.constData() +
        sizeof(decltype(priority)) +
        sizeof(decltype(weight)) +
        sizeof(decltype(port));

    auto targetLength = raw.size() -
        sizeof(decltype(priority)) -
        sizeof(decltype(weight)) -
        sizeof(decltype(port));

    target.append(position, targetLength);
}

#endif // ENABLE_MDNS

