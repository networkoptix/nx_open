#pragma once

#include "filter_params.h"

namespace nx::vms::common::transcoding {

struct TimestampParams
{
    FilterParams filterParams;
    qint64 displayOffset = 0;
    qint64 timeMs = 0;
};

} // namespace nx::vms::common::transcoding
