// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

#include <nx/sdk/analytics/i_metadata_packet.h>

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Metadata packet containing information about motion on the scene.
 */
class IMotionMetadataPacket: public Interface<IMotionMetadataPacket, IMetadataPacket>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IMotionMetadataPacket"); }

    virtual const uint8_t* motionData() const = 0;

    virtual int motionDataSize() const = 0;

    virtual int rowCount() const = 0;

    virtual int columnCount() const = 0;

    virtual bool isMotionAt(int x, int y) const = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
