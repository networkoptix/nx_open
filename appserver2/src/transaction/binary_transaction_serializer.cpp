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

    void QnBinaryTransactionSerializer::toFormattedHex(quint8* dst, quint32 payloadSize)
    {
        for (;payloadSize; payloadSize >>= 4) {
            quint8 digit = payloadSize % 16;
            *dst-- = digit < 10 ? digit + '0': digit + 'A'-10;
        }
    }

    void QnBinaryTransactionSerializer::serializeHeader(QnOutputBinaryStream<QByteArray> &stream, const QnTransactionTransportHeader& ttHeader)
    {
#ifdef _DEBUG
        foreach (const QnId& peer, ttHeader.dstPeers) {
            Q_ASSERT(!peer.isNull());
            Q_ASSERT(peer != qnCommon->moduleGUID());
        }
#endif
        stream.write("00000000\r\n",10);
        stream.write("0000", 4);
        QnBinary::serialize(ttHeader, &stream);
    }

    void QnBinaryTransactionSerializer::serializePayload(QByteArray& buffer) {
        quint32 payloadSize = buffer.size() - 12;
        quint32* payloadSizePtr = (quint32*) (buffer.data() + 10);
        *payloadSizePtr = htonl(payloadSize - 4);
        toFormattedHex((quint8*) buffer.data() + 7, payloadSize);
    }

}
