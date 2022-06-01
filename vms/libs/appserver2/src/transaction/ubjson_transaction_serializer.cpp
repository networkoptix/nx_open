// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
            NX_ASSERT(!peer.isNull());
#endif

        tranData.append((const char*) chunkPayload + stream.pos(), len - stream.pos());
        return true;
    }

    uint qHash(const QnUbjsonTransactionSerializer::CacheKey &id)
    {
        QByteArray idData(id.persistentInfo.dbID.toRfc4122());
        idData
            .append((const char*)&id.persistentInfo.timestamp, sizeof(id.persistentInfo.timestamp))
            .append((const char*)&id.persistentInfo.sequence, sizeof(id.persistentInfo.sequence));

        return ::qHash(idData, id.command);
    }
}
