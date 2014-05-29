#include "json_transaction_serializer.h"

namespace ec2 {

    void QnJsonTransactionSerializer::serializeHeader(QnOutputBinaryStream<QByteArray> stream, const QnTransactionTransportHeader & header)
    {
        stream.write("00000000\r\n",10);
        stream.write("0000", 4);
        Q_UNUSED(header)
        //QByteArray headerJson = QJson::serialized(header);    // crashes JSON structure, not required for now --gdm
        //stream.write(headerJson, headerJson.size());
    }

    void QnJsonTransactionSerializer::serializePayload(QByteArray &buffer)
    {
        quint32 payloadSize = buffer.size() - 12;
        qDebug() <<"payload size" << payloadSize;
        quint32* payloadSizePtr = (quint32*) (buffer.data() + 10);
        *payloadSizePtr = htonl(payloadSize - 4);
        toFormattedHex((quint8*) buffer.data() + 7, payloadSize);
    }

    void QnJsonTransactionSerializer::toFormattedHex(quint8* dst, quint32 payloadSize)
    {
        for (;payloadSize; payloadSize >>= 4) {
            quint8 digit = payloadSize % 16;
            *dst-- = digit < 10 ? digit + '0': digit + 'A'-10;
        }
    }

}