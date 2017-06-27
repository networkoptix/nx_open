#pragma once

#include <vector>

#include <nx/network/http/http_types.h>

namespace nx_http {

/**
 * This class encodes data into a standard-compliant byte array.
 */
class NX_NETWORK_API QnChunkedTransferEncoder
{
public:
    static QByteArray serializeSingleChunk(
        const QByteArray& data,
        const std::vector<nx_http::ChunkExtension>& chunkExtensions =
            std::vector<nx_http::ChunkExtension>());
};

} // namespace nx_http
