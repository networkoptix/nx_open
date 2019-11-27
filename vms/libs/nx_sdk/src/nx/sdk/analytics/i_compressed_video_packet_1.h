// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

#include "i_compressed_media_packet.h"

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Represents a single video frame.
 */
class ICompressedVideoPacket1: public Interface<ICompressedVideoPacket1, ICompressedMediaPacket>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::ICompressedVideoPacket"); }

    /**
     * @return Width of video frame in pixels.
     */
    virtual int width() const = 0;

    /**
     * @return Height of video frame in pixels.
     */
    virtual int height() const = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
