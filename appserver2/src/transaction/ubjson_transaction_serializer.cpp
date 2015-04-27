#include "ubjson_transaction_serializer.h"

namespace ec2
{
    bool QnUbjsonTransactionSerializer::deserializeTran(const quint8* chunkPayload, int len,  QnTransactionTransportHeader& transportHeader, QByteArray& tranData)
    {
        QByteArray srcData = QByteArray::fromRawData((const char*) chunkPayload, len);
        QnUbjsonReader<QByteArray> stream(&srcData);

        if (!QnUbjson::deserialize(&stream, &transportHeader))
            return false;

#ifdef _DEBUG
        for (const QnUuid& peer: transportHeader.dstPeers)
            assert(!peer.isNull());
#endif

        tranData.append((const char*) chunkPayload + stream.pos(), len - stream.pos());
        return true;
    }

#ifndef QN_NO_QT
    uint qHash(const QnUbjsonTransactionSerializer::CacheKey &id) 
    {
        return ::qHash(QByteArray(id.persistentInfo.dbID.toRfc4122()).
            append((const char*)&id.persistentInfo.timestamp, sizeof(id.persistentInfo.timestamp)).
            append((const char*)&id.persistentInfo.sequence, sizeof(id.persistentInfo.sequence)),
            id.command);
    }
#endif
}
