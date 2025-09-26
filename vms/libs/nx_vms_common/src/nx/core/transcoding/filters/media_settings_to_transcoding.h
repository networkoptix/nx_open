// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/core/transcoding/filters/transcoding_settings.h>
#include <nx/vms/api/data/media_settings.h>

namespace nx::core::transcoding {

NX_VMS_COMMON_API Settings fromMediaSettings(
    const QnAspectRatio& aspectRatio,
    std::optional<int> rotation,
    const QnConstResourceVideoLayoutPtr& layout,
    nx::vms::api::dewarping::MediaData dewarping,
    const nx::vms::api::MediaSettings& settings);

} // namespace nx::core::transcoding
