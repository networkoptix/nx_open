// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/network/http/http_types.h>

namespace nx::network::http {

/**
 * This class encodes data into a standard-compliant byte array.
 */
class NX_NETWORK_API QnChunkedTransferEncoder
{
public:
    static nx::Buffer serializeSingleChunk(
        const nx::ConstBufferRefType& data,
        const std::vector<nx::network::http::ChunkExtension>& chunkExtensions =
            std::vector<nx::network::http::ChunkExtension>());
};

} // namespace nx::network::http
