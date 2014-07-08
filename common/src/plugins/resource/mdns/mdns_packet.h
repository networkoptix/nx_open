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
            char queryNameArray[] = {0x05, 0x5f, 0x68, 0x74, 0x74, 0x70, 0x04, 0x5f, 0x74, 0x63, 0x70, 0x05, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x00};
            queryName = queryNameArray;

            queryType = 0xc;
            queryData.append((char)0x0);
            queryData.append((char)0x1);
        }

        QByteArray queryName;
        quint16 queryType;
        QByteArray queryData;
    };

    QnMdnsPacket()
        : transactionId(0),
        flags(0),
        questions(0),
        answerRRs(0),
        authorityRRs(0),
        additionalRRs(0)
    {
    }


    quint16 transactionId;
    quint16 flags;
    quint16 questions;
    quint16 answerRRs;
    quint16 authorityRRs;
    quint16 additionalRRs;

    QVector<Query> queries;

public:
    void addQuery();
    void toDatagram(QByteArray& datagram);
    void fromDatagram(QByteArray& datagram);
};

#endif // ENABLE_MDNS

#endif // QN_MDNS_PACKET_H

