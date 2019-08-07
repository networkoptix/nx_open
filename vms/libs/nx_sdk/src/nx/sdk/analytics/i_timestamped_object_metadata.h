// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/i_object_metadata.h>

namespace nx {
namespace sdk {
namespace analytics {

class ITimestampedObjectMetadata: public Interface<ITimestampedObjectMetadata, IObjectMetadata>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::ITimestampedObjectMetadata"); }

    virtual int64_t timestampUs() const = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
