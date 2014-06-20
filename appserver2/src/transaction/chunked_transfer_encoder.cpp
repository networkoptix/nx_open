#include "chunked_transfer_encoder.h"

#ifdef Q_OS_WIN
#   include <winsock2.h>
#else
#   include <arpa/inet.h>
#endif

#include <utils/serialization/binary_stream.h>
#include <utils/network/socket.h>


namespace ec2 {

    void toFormattedHex(quint8* dst, quint32 payloadSize) {
        for (;payloadSize; payloadSize >>= 4) {
            quint8 digit = payloadSize % 16;
            *dst-- = digit < 10 ? digit + '0': digit + 'A'-10;
        }
    }

    void serializePayload(QByteArray &buffer) {
        quint32 payloadSize = buffer.size() - 12;
        quint32* payloadSizePtr = (quint32*) (buffer.data() + 10);
        *payloadSizePtr = htonl(payloadSize - 4);
        toFormattedHex((quint8*) buffer.data() + 7, payloadSize);
    }

    QByteArray QnChunkedTransferEncoder::serializedTransaction(const QByteArray &data) {
        QByteArray result;
        QnOutputBinaryStream<QByteArray> stream(&result);
        stream.write("00000000\r\n",10);
        stream.write("0000", 4);
        stream.write(data.data(), data.size());
        stream.write("\r\n",2); // chunk end
        serializePayload(result);
        return result;
    }

}
