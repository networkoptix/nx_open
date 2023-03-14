// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <recording/time_period.h>

namespace nx::vms::client::desktop {

/** Additional data for the thumbnails search layout. */
struct ThumbnailsSearchState
{
    QnTimePeriod period;
    qint64 step = 0;
};

} // namespace nx::vms::client::desktop
