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
        const std::vector<nx_http::ChunkExtension>& chunkExtensions )
    {
        QByteArray result;
        quint32 dataSize = static_cast<quint32>(data.size());

        size_t totalExtensionsLength = 0;
        for( auto val: chunkExtensions )
            totalExtensionsLength +=
                1                       // ";"
                + val.first.size()
                + 1                     // "="
                + val.second.size();

        result.reserve(
            sizeof(dataSize)*2      //max len of hex representation of chunk size
            + totalExtensionsLength
            + sizeof("\r\n")
            + sizeof(dataSize) + dataSize
            + sizeof("\r\n") );

        //writing chunk size (hex)
        result.append( "00000000" );
        toFormattedHex( reinterpret_cast<quint8*>(result.data()) + sizeof(dataSize)*2 - 1, sizeof(dataSize) + dataSize );
        for( auto val: chunkExtensions )
        {
            result += ';';
            result += val.first;
            if( !val.second.isEmpty() )
            {
                result += "=\"";
                result += val.second;
                result += "\"";
            }
        }
        result += "\r\n";
        dataSize = htonl(dataSize);
        result += QByteArray::fromRawData( reinterpret_cast<const char*>(&dataSize), sizeof(dataSize) );
        result += data;
        result += "\r\n";
        return result;
    }
}
