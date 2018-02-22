#pragma once

#include <plugins/metadata/data_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Each class that implements MediaFrame interface
 * should properly handle this GUID in its queryInterface method
 */
static const nxpl::NX_GUID IID_MediaFrame
    = {{0x13, 0x85, 0x3c, 0xd6, 0x13, 0x7e, 0x4d, 0x8b, 0x9b, 0x8e, 0x63, 0xf1, 0x5f, 0x93, 0x2a, 0xc1}};

/**
 * @brief The MediaFrame class is an interface for decoded media frame.
 */
class MediaFrame: public DataPacket
{
public:

    /**
     * @return decoded data plane count.
     */
    virtual int planeCount() const = 0;

    /**
     * @return pointers to decoded data.
     */
    virtual const char* bits(int plane) const = 0;

    /**
     * @return bytes per line for plane.
     */
    virtual const int* bytesPerLine(int plane) const = 0;
};

} // namespace metadata
} // namesapce sdk
} // namespace nx
