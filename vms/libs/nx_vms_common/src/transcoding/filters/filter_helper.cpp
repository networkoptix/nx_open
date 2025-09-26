// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "filter_helper.h"

#include <core/resource/media_resource.h>
#include <nx/core/transcoding/filters/text_image_filter.h>
#include <nx/media/ffmpeg/frame_info.h>
#include <nx/utils/log/log.h>
#include <nx/vms/time/formatter.h>
#include <utils/common/util.h>

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
    const nx::core::transcoding::TimestampParams& params)
{
    QString result;
    const qint64 timestampMs = NX_ASSERT(params.timestamp.count() > 0)
        ? params.timestamp.count()
        : frame->pts / 1000;

    QDateTime displayTime = QDateTime::fromMSecsSinceEpoch(timestampMs, params.timeZone);

    if (timestampMs * 1000 >= UTC_TIME_DETECTION_THRESHOLD)
        result = nx::vms::time::toString(displayTime);
    else // FIXME: #sivanov Looks like duration string should be used here without offset.
        result = nx::vms::time::toString(timestampMs, nx::vms::time::Format::hh_mm_ss_zzz);

    return result;
}

} // namespace

namespace nx::core::transcoding {

FilterChainPtr createFilterChain(
    const Settings& settings,
    TimestampParams timestampParams,
    FilterParams cameraNameParams,
    QString cameraName)
{
    auto result = std::make_unique<FilterChain>(settings);
    TextImageFilter::Factor factor(1, 1);
    if (timestampParams.filterParams.enabled && cameraNameParams.enabled)
    {
        const auto timestampCorner = timestampParams.filterParams.corner;
        const auto cameraNameCorner = cameraNameParams.corner;
        if (timestampCorner == cameraNameCorner)
        {
            // Both markers should be placed in the same corner, creating single filter.
            const auto textGetter =
                [timestampParams = timestampParams, text = cameraName]
                    (const CLVideoDecoderOutputPtr& frame, int cutSymbolsCount)
                {
                    const auto cameraName = chopWithTilda(text, cutSymbolsCount);
                    if (cameraName.isEmpty())
                        cutSymbolsCount -= cameraName.length();

                    const auto timestamp = getFrameTimestampText(frame, timestampParams);
                    if (cameraName.isEmpty())
                        return chopWithTilda(timestamp, cutSymbolsCount);

                    return QString("%1\n%2").arg(
                        chopWithTilda(cameraName, cutSymbolsCount),
                        timestamp);
                };

            const auto filter = TextImageFilter::create(
                settings.layout,
                timestampParams.filterParams.corner,
                textGetter);
            result->addLegacyFilter(filter);
            return result;
        }

        if (getHorizontalEdge(timestampCorner) != getHorizontalEdge(cameraNameCorner))
            factor.setX(0.5);
        else
            factor.setY(0.5);
    }

    if (timestampParams.filterParams.enabled)
    {
        const auto textGetter =
            [timestampParams = timestampParams]
                (const CLVideoDecoderOutputPtr& frame, int cutSymbolsCount)
            {
                return chopWithTilda(getFrameTimestampText(frame, timestampParams), cutSymbolsCount);
            };

        const auto filter = TextImageFilter::create(
            settings.layout,
            timestampParams.filterParams.corner,
            textGetter,
            factor);
        result->addLegacyFilter(filter);
    }

    if (cameraNameParams.enabled)
    {
        const auto textGetter =
            [cameraName = cameraName]
                (const CLVideoDecoderOutputPtr& /*frame*/, int cutSymbolsCount)
            {
                return chopWithTilda(cameraName, cutSymbolsCount);
            };

        const auto filter = TextImageFilter::create(
            settings.layout,
            cameraNameParams.corner,
            textGetter,
            factor);
        result->addLegacyFilter(filter);
    }

    return result;
}

} // namespace nx::core::transcoding
