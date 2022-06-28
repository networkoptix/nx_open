// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <utils/media/mpeg4/video_object_layer.h>

namespace nx::media::mpeg4 {

NX_VMS_COMMON_API std::optional<VideoObjectLayer> findAndParseVolHeader(
    const uint8_t* data, int size);

} // namespace nx::media::mpeg4
