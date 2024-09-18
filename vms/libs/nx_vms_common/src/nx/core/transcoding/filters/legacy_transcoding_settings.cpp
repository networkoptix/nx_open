// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "legacy_transcoding_settings.h"

#include <core/resource/media_resource.h>
#include <core/resource/resource_media_layout.h>

namespace nx {
namespace core {
namespace transcoding {

LegacyTranscodingSettings::LegacyTranscodingSettings(
    const QnMediaResourcePtr& resource,
    const nx::vms::api::MediaSettings* settings)
    :
    resource(resource)
{
    forcedAspectRatio = resource->customAspectRatio();
    rotation = resource->forcedRotation().value_or(0);

    if (!settings)
        return;

    // Rotation.
    if (settings->rotation != "auto")
    {
        rotation = std::atoi(settings->rotation.c_str());
        NX_ASSERT(rotation == 0 || rotation == 90 || rotation == 180 || rotation == 270);
    }

    // Aspect ratio.
    if (!settings->aspectRatio.isAuto)
    {
        forcedAspectRatio = QnAspectRatio(settings->aspectRatio.size);
    }

    // Dewarping.
    if (settings->dewarping)
    {
        itemDewarpingParams.enabled = *settings->dewarping;
        forceDewarping = *settings->dewarping;
    }
    if (itemDewarpingParams.enabled)
    {
        itemDewarpingParams.xAngle = settings->dewarpingXangle;
        itemDewarpingParams.yAngle = settings->dewarpingYangle;
        itemDewarpingParams.fov = settings->dewarpingFov;
        NX_ASSERT(settings->dewarpingPanofactor == 1
            || settings->dewarpingPanofactor == 2
            || settings->dewarpingPanofactor == 4);
        itemDewarpingParams.panoFactor = settings->dewarpingPanofactor;
    }

    // Zoom.
    if (settings->zoom)
    {
        zoomWindow = *settings->zoom;
    }

    // Panoramic.
    if (settings->panoramic)
    {
        const auto layout = resource->getVideoLayout();
        if (layout && layout->channelCount() > 1)
            panoramic = true;
    }
}

bool LegacyTranscodingSettings::isEmpty() const
{
    if (forcedAspectRatio.isValid())
        return false;

    const auto layout = resource->getVideoLayout();
    if (layout && layout->channelCount() > 1)
        return false;

    if (!zoomWindow.isEmpty())
        return false;

    if (itemDewarpingParams.enabled)
        return false;

    if (contrastParams.enabled)
        return false;

    if (rotation != 0)
        return false;

    if (timestampParams.filterParams.enabled)
        return false;

    if (cameraNameParams.enabled)
        return false;

    if (watermark.visible())
        return false;

    if (panoramic)
        return false;

    return true;
}

} // namespace transcoding
} // namespace core
} // namespace nx
