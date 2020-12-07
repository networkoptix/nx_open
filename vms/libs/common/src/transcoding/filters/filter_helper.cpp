#include "filter_helper.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <core/resource/media_resource.h>
#include <nx/vms/common/transcoding/text_image_filter.h>
#include <nx/core/transcoding/filters/legacy_transcoding_settings.h>
#include <translation/datetime_formatter.h>
#include <utils/common/util.h>
#include <utils/media/frame_info.h>

namespace {

Qt::Edge getVerticalEdge(Qt::Corner corner)
{
    switch (corner)
    {
        case Qt::TopLeftCorner:
        case Qt::TopRightCorner:
            return Qt::TopEdge;
        default:
            return Qt::BottomEdge;
    }
}

QString getFrameTimestampText(
    const CLVideoDecoderOutputPtr& frame,
    const nx::vms::common::transcoding::TimestampParams& params)
{
    QString result;
    qint64 displayTime = (params.timeMs > 0)
        ? params.timeMs
        : frame->pts / 1000;

    displayTime += params.displayOffset;

    if (displayTime * 1000 >= UTC_TIME_DETECTION_THRESHOLD)
        result = datetime::toString(displayTime);
    else
        result = datetime::toString(displayTime, datetime::Format::hh_mm_ss_zzz);

    return result;
}

QString getCameraName(const QnLegacyTranscodingSettings& settings)
{
    return settings.resource->toResource()->getName();
}

} // namespace

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

    qreal widthFactor = 1.0;

    if (legacy.timestampParams.filterParams.enabled && legacy.cameraNameParams.enabled)
    {
        const auto timestampCorner = legacy.timestampParams.filterParams.corner;
        const auto cameraNameCorner = legacy.cameraNameParams.corner;
        if (timestampCorner == cameraNameCorner)
        {
            // Both markers should be placed in the same corner, creating single filter.
            const auto textGetter =
                [settings = legacy](const CLVideoDecoderOutputPtr& frame)
                {
                    return QString("%1\n%2").arg(
                        getCameraName(settings),
                        getFrameTimestampText(frame, settings.timestampParams));
                };
            const auto filter = nx::vms::common::transcoding::TextImageFilter::create(
                legacy.resource->getVideoLayout(),
                legacy.timestampParams.filterParams.corner,
                textGetter);
            result.addLegacyFilter(filter);
            return result;
        }

        if (getVerticalEdge(timestampCorner) == getVerticalEdge(cameraNameCorner))
            widthFactor = 0.5;
    }

    if (legacy.timestampParams.filterParams.enabled)
    {
        const auto textGetter =
            [settings = legacy.timestampParams](const CLVideoDecoderOutputPtr& frame)
            {
                return getFrameTimestampText(frame, settings);
            };

        const auto filter = nx::vms::common::transcoding::TextImageFilter::create(
            legacy.resource->getVideoLayout(),
            legacy.timestampParams.filterParams.corner,
            textGetter,
            widthFactor);
        result.addLegacyFilter(filter);
    }

    if (legacy.cameraNameParams.enabled)
    {
        const auto textGetter =
            [settings = legacy](const CLVideoDecoderOutputPtr& /*frame*/)
            {
                return getCameraName(settings);
            };

        const auto filter = nx::vms::common::transcoding::TextImageFilter::create(
            legacy.resource->getVideoLayout(),
            legacy.cameraNameParams.corner,
            textGetter,
            widthFactor);
        result.addLegacyFilter(filter);
    }

    return result;
}

#endif // ENABLE_DATA_PROVIDERS
