// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_settings_to_transcoding.h"

#include <core/resource/resource_media_layout.h>

namespace nx::core::transcoding {

Settings fromMediaSettings(
    const QnAspectRatio& aspectRatio,
    std::optional<int> rotation,
    const QnConstResourceVideoLayoutPtr& layout,
    nx::vms::api::dewarping::MediaData dewarping,
    const nx::vms::api::MediaSettings& settings)
{
    Settings result;
    result.aspectRatio = aspectRatio;
    result.rotation = rotation.value_or(0);
    result.layout = layout;
    result.dewarpingMedia = dewarping;

    // Rotation.
    if (settings.rotation != "auto")
    {
        result.rotation = std::atoi(settings.rotation.c_str());
        NX_ASSERT(result.rotation == 0 || result.rotation == 90
            || result.rotation == 180 || result.rotation == 270);
    }

    // Aspect ratio.
    if (!settings.aspectRatio.isAuto)
        result.aspectRatio = QnAspectRatio(settings.aspectRatio.size);

    // Dewarping.
    if (settings.dewarping)
    {
        result.dewarping.enabled = *settings.dewarping;
        result.dewarpingMedia.enabled = *settings.dewarping;
    }

    if (result.dewarping.enabled)
    {
        result.dewarping.xAngle = settings.dewarpingXangle;
        result.dewarping.yAngle = settings.dewarpingYangle;
        result.dewarping.fov = settings.dewarpingFov;
        NX_ASSERT(settings.dewarpingPanofactor == 1
            || settings.dewarpingPanofactor == 2
            || settings.dewarpingPanofactor == 4);
        result.dewarping.panoFactor = settings.dewarpingPanofactor;
    }

    // Zoom.
    if (settings.zoom)
        result.zoomWindow = *settings.zoom;

    return result;
}

} // namespace nx::core::transcoding
