#include "binary_transaction_serializer.h"

#ifdef _DEBUG
    #include <common/common_module.h>
#endif

namespace ec2
{
    bool QnBinaryTransactionSerializer::deserializeTran(const quint8* chunkPayload, int len,  QnTransactionTransportHeader& transportHeader, QByteArray& tranData)
    {
        QByteArray srcData = QByteArray::fromRawData((const char*) chunkPayload, len);
        QnInputBinaryStream<QByteArray> stream(srcData);

        if (!deserialize(&stream, &transportHeader))
            return false;

#ifdef _DEBUG
        foreach (const QnId& peer, transportHeader.dstPeers)
            Q_ASSERT(!peer.isNull());
#endif

        tranData.append((const char*) chunkPayload + stream.pos(), len - stream.pos());
        return true;
    }



    void QnBinaryTransactionSerializer::serializeHeader(QnOutputBinaryStream<QByteArray> &stream, const QnTransactionTransportHeader& ttHeader)
    {
#ifdef _DEBUG
        foreach (const QnId& peer, ttHeader.dstPeers) {
            Q_ASSERT(!peer.isNull());
            Q_ASSERT(peer != qnCommon->moduleGUID());
        }
#endif
        QnBinary::serialize(ttHeader, &stream);
    }
}
