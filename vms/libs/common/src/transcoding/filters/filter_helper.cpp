#include "filter_helper.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <core/resource/media_resource.h>

#include <transcoding/filters/time_image_filter.h>

#include <nx/core/transcoding/filters/legacy_transcoding_settings.h>

nx::core::transcoding::FilterChain QnImageFilterHelper::createFilterChain(
    const nx::core::transcoding::LegacyTranscodingSettings& legacy)
{
    nx::core::transcoding::Settings settings;
    settings.aspectRatio = legacy.forcedAspectRatio;
    settings.rotation = legacy.rotation;
    settings.dewarping = legacy.itemDewarpingParams;
    settings.enhancement = legacy.contrastParams;
    settings.zoomWindow = legacy.zoomWindow;
    settings.watermark = legacy.watermark;

    nx::core::transcoding::FilterChain result(
        settings, legacy.resource->getDewarpingParams(), legacy.resource->getVideoLayout());
    if (legacy.timestampParams.enabled)
    {
        result.addLegacyFilter(QnAbstractImageFilterPtr(new QnTimeImageFilter(
            legacy.resource->getVideoLayout(), legacy.timestampParams)));
    }

    return result;
}

#endif // ENABLE_DATA_PROVIDERS
