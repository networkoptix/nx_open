// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

#include <nx/sdk/analytics/i_metadata.h>

namespace nx {
namespace sdk {
namespace analytics {

// TODO: Add detailed description of the motion grid.
class IMotionMetadata: public Interface<IMotionMetadata, IMetadata>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IMotionMetadata"); }

    virtual const uint8_t* motionData() const = 0;

    virtual int motionDataSize() const = 0;

    virtual int rowCount() const = 0;

    virtual int columnCount() const = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
