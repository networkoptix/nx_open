#include "transaction_transport_serializer.h"

#include <common/common_module.h>
#include <utils/common/model_functions.h>

#include "transaction_message_bus.h"

namespace ec2
{
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS(TransactionTransportHeader, (binary)(json), TransactionTransportHeader_Fields);


    QnTransactionTransportSerializer::QnTransactionTransportSerializer(QnTransactionMessageBus& owner): m_owner(owner)
    {

    }

    bool QnTransactionTransportSerializer::deserializeTran(const quint8* chunkPayload, int len,  TransactionTransportHeader& transportHeader, QByteArray& tranData)
    {
        QByteArray srcData = QByteArray::fromRawData((const char*) chunkPayload, len);
        QnInputBinaryStream<QByteArray> stream(&srcData);

        if (!deserialize(&stream, &transportHeader))
            return false;

#ifdef _DEBUG
        foreach (const QnId& peer, transportHeader.dstPeers)
            Q_ASSERT(!peer.isNull());
#endif

        tranData.append((const char*) chunkPayload + stream.pos(), len - stream.pos());
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
