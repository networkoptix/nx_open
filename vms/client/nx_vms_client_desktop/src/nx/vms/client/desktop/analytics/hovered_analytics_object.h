// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <string>

#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

/** Analytics object under the cursor on a video item. */
struct HoveredAnalyticsObject
{
    nx::Uuid trackId;
    std::string objectTypeId;

    /** Timestamp of the video frame the object was shown on. */
    std::chrono::microseconds timestamp{};
};

} // namespace nx::vms::client::desktop
