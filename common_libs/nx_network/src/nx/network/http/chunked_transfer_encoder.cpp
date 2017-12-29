#include "chunked_transfer_encoder.h"

#ifdef Q_OS_WIN
#   include <winsock2.h>
#else
#   include <arpa/inet.h>
#endif

#include <nx/network/socket.h>
#include <nx/fusion/serialization/binary_stream.h>

namespace nx {
namespace network {
namespace http {

QByteArray QnChunkedTransferEncoder::serializeSingleChunk(
    const QByteArray& data,
    const std::vector<nx::network::http::ChunkExtension>& chunkExtensions)
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

} // namespace nx
} // namespace network
} // namespace http
