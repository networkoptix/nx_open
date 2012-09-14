#include "mdns.h"

void MDNSPacket::toDatagram(QByteArray& datagram)
{
    QDataStream stream(&datagram, QIODevice::WriteOnly);
    stream << transactionId << flags << questions << answerRRs << authorityRRs << additionalRRs;

    foreach (const Query& query, queries)
    {
        stream.writeRawData(query.queryName.data(), query.queryName.size());
        stream << (quint8)0;
        stream << query.queryType;
        stream.writeRawData(query.queryData.data(), query.queryData.size());
    }
}

void MDNSPacket::fromDatagram(QByteArray& datagram)
{
    QDataStream stream(&datagram, QIODevice::ReadOnly);
    stream >> transactionId >> flags >> questions >> answerRRs >> authorityRRs >> additionalRRs;

    for (int i = 0; i < answerRRs; i++)
    {
        Query query;
        stream >> query.queryName >> query.queryType;

        switch (query.queryType)
        {
        case 0x0c:
            {
                stream.skipRawData(6);
                quint16 dataLen;
                stream >> dataLen;
                char* data = new char[dataLen];
                stream.readRawData(data, dataLen);
                delete[] data;
                break;
            }
        }
        //query.queryData;
    }

    for (int i = 0; i < authorityRRs; i++)
    {
        Query query;
        stream >> query.queryName >> query.queryType >> query.queryData;
    }

    for (int i = 0; i < additionalRRs; i++)
    {
        Query query;
        stream >> query.queryName >> query.queryType >> query.queryData;
    }
}

void MDNSPacket::addQuery()
{
    queries.append(Query());
    questions++;
}
