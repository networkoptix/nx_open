
#include "chunked_transfer_encoder.h"

#ifdef Q_OS_WIN
#   include <winsock2.h>
#else
#   include <arpa/inet.h>
#endif

#include <nx/network/socket.h>
#include <nx/fusion/serialization/binary_stream.h>


namespace nx_http {

namespace {
/** Writes hex representation of \a payloadSize to \a dst. 
    \a dst is a pointer to right-most digit, so there MUST be enough space before \a dst
*/
static void toFormattedHex(quint8* dst, quint32 payloadSize)
{
    for (; payloadSize; payloadSize >>= 4)
    {
        quint8 digit = payloadSize & 0x0f;
        *dst-- = digit < 10 ? digit + '0' : digit + 'A' - 10;
    }
}
}

QByteArray QnChunkedTransferEncoder::serializeSingleChunk(
    const QByteArray& data,
    const std::vector<nx_http::ChunkExtension>& chunkExtensions)
{
    QByteArray result;
    const quint32 dataSize = static_cast<quint32>(data.size());

    size_t totalExtensionsLength = 0;
    for (auto val : chunkExtensions)
        totalExtensionsLength +=
        1                       // ";"
        + val.first.size()
        + 1                     // "="
        + val.second.size();

    result.reserve(
        sizeof(dataSize) * 2      //max len of hex representation of chunk size
        + totalExtensionsLength
        + sizeof("\r\n")
        + dataSize
        + sizeof("\r\n"));

    //writing chunk size (hex)
    result += QByteArray::number(dataSize, 16);
    for (auto val : chunkExtensions)
    {
        result += ';';
        result += val.first;
        if (!val.second.isEmpty())
        {
            result += "=\"";
            result += val.second;
            result += "\"";
        }
    }
    result += "\r\n";
    result += data;
    result += "\r\n";
    return result;
}

}   //namespace nx_http
