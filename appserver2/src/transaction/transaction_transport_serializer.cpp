#include "transaction_transport_serializer.h"
#include "common/common_module.h"
#include "transaction_message_bus.h"

namespace ec2
{

    QnTransactionTransportSerializer::QnTransactionTransportSerializer(QnTransactionMessageBus& owner): m_owner(owner)
    {

    }

    bool QnTransactionTransportSerializer::deserializeTran(const quint8* chunkPayload, int len,  PeerList& processedPeers, PeerList& dstPeers, QByteArray& tranData)
    {
        QByteArray srcData = QByteArray::fromRawData((const char*) chunkPayload, len);
        InputBinaryStream<QByteArray> stream(srcData);
        if (!deserialize(processedPeers, &stream))
            return false;
        if (!deserialize(dstPeers, &stream))
            return false;
        foreach (const QnId& peer, dstPeers)
            Q_ASSERT(!peer.isNull());

        tranData.append((const char*) chunkPayload + stream.getPos(), len - stream.getPos());
        return true;
    }

    void QnTransactionTransportSerializer::toFormattedHex(quint8* dst, quint32 payloadSize)
    {
        for (;payloadSize; payloadSize >>= 4) {
            quint8 digit = payloadSize % 16;
            *dst-- = digit < 10 ? digit + '0': digit + 'A'-10;
        }
    }

}
