// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "sized_data_decoder.h"

#include <cstring>

#include <nx/utils/system_network_headers.h>

namespace nx {
namespace utils {
namespace bstream {

bool SizedDataDecodingFilter::processData(const ConstBufferRefType& data)
{
    // TODO: #akolesnikov Introduce streaming implementation.

    ConstBufferRefType::size_type pos = 0;
    for (; pos < data.size(); )
    {
        uint32_t dataSize = 0;
        if (data.size() - pos < sizeof(dataSize))
            return false;
        memcpy(&dataSize, data.data() + pos, sizeof(dataSize));
        pos += sizeof(dataSize);
        dataSize = ntohl(dataSize);

        if (data.size() - pos < dataSize)
            return false;
        if (!nextFilter()->processData(data.substr(pos, dataSize)))
            return false;
        pos += dataSize;
    }

    return pos == data.size();
}

} // namespace bstream
} // namespace utils
} // namespace nx
