#include "chunked_transfer_encoder.h"

#ifdef Q_OS_WIN
#   include <winsock2.h>
#else
#   include <arpa/inet.h>
#endif

#include <utils/network/socket.h>
#include <utils/serialization/binary_stream.h>


namespace ec2 {

    //!Writes hex representation of \a payloadSize to \a dst. \a dst is a pointer to right-most digit, so there MUST be enough space before \a dst
    void toFormattedHex(quint8* dst, quint32 payloadSize) {
        for (;payloadSize; payloadSize >>= 4) {
            quint8 digit = payloadSize & 0x0f;
            *dst-- = digit < 10 ? digit + '0': digit + 'A'-10;
        }
    }

    //void serializePayload(QByteArray &buffer) {
    //    quint32 payloadSize = buffer.size() - 12;
    //    quint32* payloadSizePtr = (quint32*) (buffer.data() + 10);
    //    *payloadSizePtr = htonl(payloadSize - 4);
    //    toFormattedHex((quint8*) buffer.data() + 7, payloadSize);
    //}

    QByteArray QnChunkedTransferEncoder::serializedTransaction(
        const QByteArray& data,
        const QByteArray& chunkExtensionName,
        const QByteArray& chunkExtensionValue )
    {
        QByteArray result;
        quint32 dataSize = static_cast<quint32>(data.size());

        result.reserve(
            sizeof(dataSize)*2      //max len of hex representation of chunk size
            + 1     // ";"
            + chunkExtensionName.size()
            + 1     // "="
            + chunkExtensionValue.size()
            + sizeof("\r\n")
            + sizeof(dataSize) + dataSize
            + sizeof("\r\n") );

        //writing chunk size (hex)
        result.append( "00000000" );
        toFormattedHex( reinterpret_cast<quint8*>(result.data()) + sizeof(dataSize)*2 - 1, sizeof(dataSize) + dataSize );
        if( !chunkExtensionName.isEmpty() )
        {
            result += ';';
            result += chunkExtensionName;
            result += '=';
            result += chunkExtensionValue;
        }
        result += "\r\n";
        dataSize = htonl(dataSize);
        result += QByteArray::fromRawData( reinterpret_cast<const char*>(&dataSize), sizeof(dataSize) );
        result += data;
        result += "\r\n";
        return result;


        //QByteArray result;
        //QnOutputBinaryStream<QByteArray> stream(&result);
        //stream.write("00000000\r\n",10);
        //stream.write("0000", 4);
        //stream.write(data.constData(), data.size());
        //stream.write("\r\n",2); // chunk end
        //serializePayload(result);
        //return result;
    }
}
