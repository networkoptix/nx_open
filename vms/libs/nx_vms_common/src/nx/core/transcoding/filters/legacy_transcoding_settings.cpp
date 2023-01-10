// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "legacy_transcoding_settings.h"

#include <core/resource/media_resource.h>
#include <core/resource/resource_media_layout.h>

namespace nx {
namespace core {
namespace transcoding {

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

    return true;
}

} // namespace transcoding
} // namespace core
} // namespace nx
