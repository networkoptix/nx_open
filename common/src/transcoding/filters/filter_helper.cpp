#include "filter_helper.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <core/resource/media_resource.h>

#include <transcoding/filters/time_image_filter.h>

#include <nx/core/transcoding/filters/legacy_transcoding_settings.h>

nx::core::transcoding::FilterChain QnImageFilterHelper::createFilterChain(
    const nx::core::transcoding::LegacyTranscodingSettings& legacy,
    const QSize& srcFrameResolution,
    const QSize& resolutionLimit)
{
    nx::core::transcoding::Settings settings;
    if (!qFuzzyIsNull(legacy.forcedAspectRatio))
        settings.aspectRatio = QnAspectRatio::closestStandardRatio(legacy.forcedAspectRatio);
    settings.rotation = legacy.rotation;
    settings.dewarping = legacy.itemDewarpingParams;
    settings.enhancement = legacy.contrastParams;
    settings.zoomWindow = legacy.zoomWindow;

    nx::core::transcoding::FilterChain result(settings);
    result.prepare(legacy.resource, srcFrameResolution, resolutionLimit);

    if (legacy.timestampParams.enabled)
    {
        result << QnAbstractImageFilterPtr(new QnTimeImageFilter(
            legacy.resource->getVideoLayout(), legacy.timestampParams));
    }

    return result;
}

#endif // ENABLE_DATA_PROVIDERS
