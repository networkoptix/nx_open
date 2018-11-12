#pragma once

#include <nx/sdk/analytics/data_packet.h>

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Each class that implements MediaFrame interface should properly handle this GUID in its
 * queryInterface().
 */
static const nxpl::NX_GUID IID_MediaFrame =
    {{0x13,0x85,0x3c,0xd6,0x13,0x7e,0x4d,0x8b,0x9b,0x8e,0x63,0xf1,0x5f,0x93,0x2a,0xc1}};

/**
 * Interface for a decoded media frame, e.g. video or audio.
 */
class MediaFrame: public DataPacket
{
public:
    /**
     * @return Number of planes with contain war byte data. For video frame, each plane contains
     * pixel data for a particular channel (e.g. Red, Green, Blue, or Alpha channel). 0 in case
     * the raw byte data is not accessible for this type of frame.
     */
    virtual int planeCount() const = 0;

    /**
     * @param plane Number of the plane, in range 0..planesCount().
     * @return Number of bytes accessible via data(plane), or 0 if the data is not accessible.
     */
    virtual int dataSize(int plane) const = 0;

    /**
     * @param plane Number of the plane, in range 0..planesCount().
     * @return Pointer to the byte data of the plane, or null if the data is not accessible.
     */
    virtual const char* data(int plane) const = 0;
};

} // namespace analytics
} // namesapce sdk
} // namespace nx
