// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/core/transcoding/filters/filter_chain.h>
#include <nx/core/transcoding/filters/timestamp_params.h>

namespace nx::core::transcoding {

/**
 * Create filters for source image processing
 */
NX_VMS_COMMON_API FilterChainPtr createFilterChain(
    const Settings& settings,
    TimestampParams timestampParams,
    FilterParams cameraNameParams,
    QString cameraName);

} // namespace nx::core::transcoding
