#pragma once

#include <vector>

#include <nx/network/http/http_types.h>

namespace nx {
namespace network {
namespace http {

/**
 * This class encodes data into a standard-compliant byte array.
 */
class NX_NETWORK_API QnChunkedTransferEncoder
{
public:
    static QByteArray serializeSingleChunk(
        const QByteArray& data,
        const std::vector<nx::network::http::ChunkExtension>& chunkExtensions =
            std::vector<nx::network::http::ChunkExtension>());
};

} // namespace nx
} // namespace network
} // namespace http
