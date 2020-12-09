#include "filter_helper.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <core/resource/media_resource.h>
#include <nx/vms/common/transcoding/text_image_filter.h>
#include <nx/core/transcoding/filters/legacy_transcoding_settings.h>
#include <translation/datetime_formatter.h>
#include <utils/common/util.h>
#include <utils/media/frame_info.h>

namespace {

Qt::Edge getHorizontalEdge(Qt::Corner corner)
{
    switch (corner)
    {
        case Qt::TopLeftCorner:
        case Qt::BottomLeftCorner:
            return Qt::LeftEdge;
        default:
            return Qt::RightEdge;
    }
}

QString chopWithTilda(const QString& source, int symbolsCount)
{
    if (symbolsCount == 0)
        return source;

    return source.length() <= symbolsCount
        ? QString()
        : source.left(source.length() - symbolsCount - 1) + "~";
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

    using nx::vms::common::transcoding::TextImageFilter;
    TextImageFilter::Factor factor(1, 1);

    if (legacy.timestampParams.filterParams.enabled && legacy.cameraNameParams.enabled)
    {
        const auto timestampCorner = legacy.timestampParams.filterParams.corner;
        const auto cameraNameCorner = legacy.cameraNameParams.corner;
        if (timestampCorner == cameraNameCorner)
        {
            // Both markers should be placed in the same corner, creating single filter.
            const auto textGetter =
                [settings = legacy]
                    (const CLVideoDecoderOutputPtr& frame, int cutSymbolsCount)
                {
                    const auto cameraName = chopWithTilda(getCameraName(settings), cutSymbolsCount);
                    if (cameraName.isEmpty())
                        cutSymbolsCount -= cameraName.length();

                    const auto timestamp = getFrameTimestampText(frame, settings.timestampParams);
                    if (cameraName.isEmpty())
                        return chopWithTilda(timestamp, cutSymbolsCount);

                    return QString("%1\n%2").arg(
                        chopWithTilda(cameraName, cutSymbolsCount),
                        timestamp);
                };
            const auto filter = TextImageFilter::create(
                legacy.resource->getVideoLayout(),
                legacy.timestampParams.filterParams.corner,
                textGetter);
            result.addLegacyFilter(filter);
            return result;
        }

        if (getHorizontalEdge(timestampCorner) != getHorizontalEdge(cameraNameCorner))
            factor.setX(0.5);
        else
            factor.setY(0.5);
    }

    if (legacy.timestampParams.filterParams.enabled)
    {
        const auto textGetter =
            [settings = legacy.timestampParams]
                (const CLVideoDecoderOutputPtr& frame, int cutSymbolsCount)
            {
                return chopWithTilda(getFrameTimestampText(frame, settings), cutSymbolsCount);
            };

        const auto filter = TextImageFilter::create(
            legacy.resource->getVideoLayout(),
            legacy.timestampParams.filterParams.corner,
            textGetter,
            factor);
        result.addLegacyFilter(filter);
    }

    if (legacy.cameraNameParams.enabled)
    {
        const auto textGetter =
            [settings = legacy]
                (const CLVideoDecoderOutputPtr& /*frame*/, int cutSymbolsCount)
            {
                return chopWithTilda(getCameraName(settings), cutSymbolsCount);
            };

        const auto filter = TextImageFilter::create(
            legacy.resource->getVideoLayout(),
            legacy.cameraNameParams.corner,
            textGetter,
            factor);
        result.addLegacyFilter(filter);
    }

    return result;
}

#endif // ENABLE_DATA_PROVIDERS
