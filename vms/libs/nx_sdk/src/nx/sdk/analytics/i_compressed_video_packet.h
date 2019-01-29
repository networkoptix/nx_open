#pragma once

#include "i_compressed_media_packet.h"

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Each class that implements ICompressedVideoPacket interface should properly handle this GUID in
 * its queryInterface method
 */
static const nxpl::NX_GUID IID_CompressedVideoPacket =
    {{0xB6,0x39,0xE4,0x68,0x0D,0x95,0x49,0x76,0xA7,0xC3,0x68,0x4B,0xCC,0x4D,0x90,0xB9}};

class ICompressedVideoPacket: public ICompressedMediaPacket
{
public:
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
