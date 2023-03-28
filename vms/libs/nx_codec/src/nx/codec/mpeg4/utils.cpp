// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <nx/codec/nal_units.h>
#include <nx/utils/log/log.h>

namespace nx::media::mpeg4 {

std::optional<VideoObjectLayer> findAndParseVolHeader(const uint8_t* data, int size)
{
    // This function cannot be used for all mpeg4 headers, but it is acceptable for VideoObjectLayer.
    auto nalUnits = nal::findNalUnitsAnnexB(data, size);
    for (const auto& nal: nalUnits)
    {
        if (nal.size > 1 && nal.data[0] == 0x20) //< VideoObjectLayer
        {
            VideoObjectLayer result;
            if (result.read(nal.data + 1, nal.size - 1))
                return result;
        }
    }
    return std::nullopt;
}

} // namespace nx::media::mpeg4
