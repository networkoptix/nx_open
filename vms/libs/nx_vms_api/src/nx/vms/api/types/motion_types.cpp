// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "motion_types.h"

#include <nx/utils/log/assert.h>

namespace nx::vms::api {

StreamIndex oppositeStreamIndex(StreamIndex streamIndex)
{
    switch (streamIndex)
    {
        case StreamIndex::primary:
            return StreamIndex::secondary;
        case StreamIndex::secondary:
            return StreamIndex::primary;
        default:
            break;
    }
    NX_ASSERT(false, "Unsupported StreamIndex %1", streamIndex);
    return StreamIndex::undefined; //< Fallback for the failed assertion.
}

} // namespace nx::vms::api
