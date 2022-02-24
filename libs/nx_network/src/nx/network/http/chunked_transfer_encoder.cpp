// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "chunked_transfer_encoder.h"

#ifdef Q_OS_WIN
#   include <winsock2.h>
#else
#   include <arpa/inet.h>
#endif

#include <nx/network/socket.h>

namespace nx::network::http {

nx::Buffer QnChunkedTransferEncoder::serializeSingleChunk(
    const nx::ConstBufferRefType& data,
    const std::vector<ChunkExtension>& chunkExtensions)
{
    nx::Buffer result;
    const auto dataSize = data.size();

    size_t totalExtensionsLength = 0;
    for (auto val : chunkExtensions)
    {
        totalExtensionsLength +=
        1                       // ";"
        + val.first.size()
        + 1                     // "="
        + val.second.size();
    }

    result.reserve(
        sizeof(dataSize) * 2      //max len of hex representation of chunk size
        + totalExtensionsLength
        + sizeof("\r\n")
        + dataSize
        + sizeof("\r\n"));

    // Writing chunk size (hex).
    result += nx::utils::toHex(dataSize);
    for (auto val: chunkExtensions)
    {
        result += ';';
        result += val.first;
        if (!val.second.empty())
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

} // namespace nx::network::http
