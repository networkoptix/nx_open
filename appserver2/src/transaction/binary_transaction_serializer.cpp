
#if 0

#include "binary_transaction_serializer.h"

namespace ec2
{
    bool QnBinaryTransactionSerializer::deserializeTran(const quint8* chunkPayload, int len,  QnTransactionTransportHeader& transportHeader, QByteArray& tranData)
    {
        QByteArray srcData = QByteArray::fromRawData((const char*) chunkPayload, len);
        QnInputBinaryStream<QByteArray> stream(&srcData);

        if (!QnBinary::deserialize(&stream, &transportHeader))
            return false;

#ifdef _DEBUG
        foreach (const QUuid& peer, transportHeader.dstPeers)
            assert(!peer.isNull());
#endif

        tranData.append((const char*) chunkPayload + stream.pos(), len - stream.pos());
        return true;
    }
}

#endif
