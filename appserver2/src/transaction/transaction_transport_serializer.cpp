#include "transaction_transport_serializer.h"
#include "common/common_module.h"
#include "transaction_message_bus.h"

namespace ec2
{

    QnTransactionTransportSerializer::QnTransactionTransportSerializer(QnTransactionMessageBus& owner): m_owner(owner)
    {

    }

    ProcessedPeers QnTransactionTransportSerializer::updatePeers(const ProcessedPeers& opaque)
    {
        ProcessedPeers result = opaque;
        result << qnCommon->moduleGUID();
        result += m_owner.alivePeers();
        return result;
    }

    bool QnTransactionTransportSerializer::deserialize(const quint8* chunkPayload, int len,  ProcessedPeers& peers, QByteArray& tranData)
    {
        QByteArray srcData = QByteArray::fromRawData((const char*) chunkPayload, len);
        InputBinaryStream<QByteArray> stream(srcData);
        if (!QnBinary::deserialize(peers, &stream))
            return false;
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
