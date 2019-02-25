#pragma once

#include <nx/sdk/interface.h>

#include "i_compressed_media_packet.h"

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Represents a single video frame.
 */
class ICompressedVideoPacket: public Interface<ICompressedVideoPacket, ICompressedMediaPacket>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::analytics::ICompressedVideoPacket"); }

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
