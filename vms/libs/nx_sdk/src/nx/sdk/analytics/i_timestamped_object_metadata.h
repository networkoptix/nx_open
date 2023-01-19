// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/i_object_metadata.h>

namespace nx::sdk::analytics {

class ITimestampedObjectMetadata: public Interface<ITimestampedObjectMetadata, IObjectMetadata0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::ITimestampedObjectMetadata"); }

    /** @return A positive value. */
    virtual int64_t timestampUs() const = 0;
};
using ITimestampedObjectMetadata0 = ITimestampedObjectMetadata;

} // namespace nx::sdk::analytics
