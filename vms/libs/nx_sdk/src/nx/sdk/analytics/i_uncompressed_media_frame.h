#pragma once

#include <nx/sdk/interface.h>

#include <nx/sdk/analytics/i_data_packet.h>

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Decoded media frame, e.g. video or audio.
 */
class IUncompressedMediaFrame: public Interface<IUncompressedMediaFrame, IDataPacket>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::analytics::IUncompressedMediaFrame"); }

    /**
     * @return Number of planes with contain war byte data. For video frame, each plane contains
     * pixel data for a particular channel (e.g. Red, Green, Blue, or Alpha channel). 0 in case
     * the raw byte data is not accessible for this type of frame.
     */
    virtual int planeCount() const = 0;

    /**
     * @param plane Number of the plane, in range 0..planeCount().
     * @return Number of bytes accessible via data(plane), or 0 if the data is not accessible.
     */
    virtual int dataSize(int plane) const = 0;

    /**
     * @param plane Number of the plane, in range 0..planeCount().
     * @return Pointer to the byte data of the plane, or null if the data is not accessible.
     */
    virtual const char* data(int plane) const = 0;
};

} // namespace analytics
} // namesapce sdk
} // namespace nx
