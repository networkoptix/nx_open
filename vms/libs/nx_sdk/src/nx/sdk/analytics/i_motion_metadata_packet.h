// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/i_metadata_packet.h>
#include <nx/sdk/interface.h>

namespace nx::sdk::analytics {

/**
 * Metadata packet containing information about motion on the scene.
 */
class IMotionMetadataPacket: public Interface<IMotionMetadataPacket, IMetadataPacket0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IMotionMetadataPacket"); }

    /**
     * Motion metadata is represented as a grid with a certain number of rows and columns. The grid
     * coordinate system starts at the top left corner, the row index grows down and the column
     * index grows right. This grid is represented as a contiguous array, each bit of which
     * corresponds to the state of a particular cell of the grid (1 if motion has been detected
     * in this cell, 0 otherwise). The bit index for a cell with coordinates (column, row) can be
     * calculated using the following formula: bitNumber = gridHeight * column + row.
     *
     * @return Pointer to the grid buffer.
     */
    virtual const uint8_t* motionData() const = 0;

    /**
     * @return Size of the motion metadata buffer in bytes. Number of bits in the grid buffer may
     *     be more than the number of bits needed to store the whole grid, values of those
     *     excessive bits are undefined and should be ignored.
     */
    virtual int motionDataSize() const = 0;

    /**
     * @return Row count of the motion detector grid.
     */
    virtual int rowCount() const = 0;

    /**
     * @return Column count of the motion detector grid.
     */
    virtual int columnCount() const = 0;

    /**
     * @return true if motion has been detected at least in one cell, false otherwise.
     */
    virtual bool isEmpty() const = 0;

    /**
     * @return Whether motion has been detected in the specified cell.
     */
    virtual bool isMotionAt(int columnIndex, int rowIndex) const = 0;
};
using IMotionMetadataPacket0 = IMotionMetadataPacket;

} // namespace nx::sdk::analytics
