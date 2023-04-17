// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/i_data_packet.h>
#include <nx/sdk/interface.h>

namespace nx::sdk::analytics {

/**
 * Decoded media frame, e.g. video or audio.
 */
class IUncompressedMediaFrame: public Interface<IUncompressedMediaFrame, IDataPacket0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IUncompressedMediaFrame"); }

    /**
     * @return Number of planes which contain raw byte data.
     */
    virtual int planeCount() const = 0;

    /**
     * @param plane Index of the plane, in range 0..planeCount() - 1.
     * @return Number of bytes accessible via data(plane), or 0 if the data is not accessible.
     */
    virtual int dataSize(int plane) const = 0;

    /**
     * @param plane Index of the plane, in range 0..planeCount() - 1.
     * @return Pointer to the byte data of the plane, or null if the data is not accessible.
     */
    virtual const char* data(int plane) const = 0;
};
using IUncompressedMediaFrame0 = IUncompressedMediaFrame;

} // namespace nx::sdk::analytics
