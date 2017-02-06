#ifndef QN_MDNS_PACKET_H
#define QN_MDNS_PACKET_H

#ifdef ENABLE_MDNS

#include <QtCore/QByteArray>
#include <QtCore/QVector>

struct QnMdnsPacket
{
    struct Query
    {
        Query()
        {
            // By default fill records for request
            char queryNameArray[] = {
                0x05, 0x5f, 0x68, 0x74, 0x74, 0x70, 0x04, 0x5f,
                0x74, 0x63, 0x70, 0x05, 0x6c, 0x6f, 0x63, 0x61,
                0x6c, 0x00};

            queryName = queryNameArray;
            queryType = 0xc;
            queryData.append((char)0x0);
            queryData.append((char)0x1);
        }

        QByteArray queryName;
        quint16 queryType;
        quint16 queryClass;
        QByteArray queryData;
    };

    struct ResourceRecord
    {
        QByteArray recordName;
        quint16 recordType;
        quint16 recordClass;
        quint32 ttl;
        quint16 dataLength;
        QByteArray data;
    };

    QnMdnsPacket()
        : transactionId(0),
        flags(0),
        questionCount(0),
        answerRRCount(0),
        authorityRRCount(0),
        additionalRRCount(0)
    {
    }


    static const quint16 kSrvRecordType = 0x21;
    static const quint16 kPtrRecordType = 0x0c;
    static const quint16 kARecordType = 0x01;
    static const quint16 kTextRecordType = 0x10;


    quint16 transactionId;
    quint16 flags;
    quint16 questionCount;
    quint16 answerRRCount;
    quint16 authorityRRCount;
    quint16 additionalRRCount;

    QVector<Query> queries;
    QVector<ResourceRecord> answerRRs;
    QVector<ResourceRecord> authorityRRs;
    QVector<ResourceRecord> additionalRRs;

public:
    void addQuery();
    void toDatagram(QByteArray& datagram);
    bool fromDatagram(const QByteArray& datagram);

private:
    int parseName(
            const QByteArray& message,
            int nameOffset,
            QByteArray& name);

    void parseNameInternal(
            const QByteArray& message,
            int nameOffset,
            QByteArray& name,
            int& nameSize,
            bool readByPointer);
};

struct QnMdnsSrvData
{
    quint16 priority;
    quint16 weight;
    quint16 port;
    QByteArray target;
public:
    void decode(const QByteArray& data);
};

#endif // ENABLE_MDNS

#endif // QN_MDNS_PACKET_H

