#include "filter_helper.h"

#ifdef ENABLE_DATA_PROVIDERS

#include "scale_image_filter.h"
#include "tiled_image_filter.h"
#include "crop_image_filter.h"
#include "fisheye_image_filter.h"
#include "contrast_image_filter.h"
#include "rotate_image_filter.h"
#include "time_image_filter.h"
#include "../transcoder.h"

#include <nx/core/transcoding/filters/legacy_transcoding_settings.h>

QSize QnImageFilterHelper::applyFilterChain(const QnAbstractImageFilterList& filters,
    const QSize& resolution)
{
    QSize result(resolution);
    for (auto filter: filters)
        result = filter->updatedResolution(result);
    return result;
}

const QSize QnImageFilterHelper::kDefaultResolutionLimit(8192 - 16, 8192 - 16);

QnAbstractImageFilterList QnImageFilterHelper::createFilterChain(
    const nx::core::transcoding::LegacyTranscodingSettings& settings,
    const QSize& srcResolution,
    const QSize& resolutionLimit)
{
    static const float MIN_STEP_CHANGE_COEFF = 0.95f;
    static const float ASPECT_RATIO_COMPARISION_PRECISION = 0.01f;

    QList<QnAbstractImageFilterPtr> result;

    if (!qFuzzyIsNull(settings.forcedAspectRatio))
    {
        QSize newSize;
        newSize.setHeight(srcResolution.height());
        newSize.setWidth(srcResolution.height() * settings.forcedAspectRatio + 0.5);
        if (newSize != srcResolution)
            result << QnAbstractImageFilterPtr(new QnScaleImageFilter(QnCodecTranscoder::roundSize(newSize)));
    }

    bool tiledFilterExist = false;
    if (settings.layout && settings.layout->channelCount() > 1)
    {
        result << QnAbstractImageFilterPtr(new QnTiledImageFilter(settings.layout));
        tiledFilterExist = true;
    }

    if (!settings.zoomWindow.isEmpty() && !settings.itemDewarpingParams.enabled)
    {
        result << QnAbstractImageFilterPtr(new QnCropImageFilter(settings.zoomWindow));
    }
    if (settings.itemDewarpingParams.enabled)
        result << QnAbstractImageFilterPtr(new QnFisheyeImageFilter(settings.mediaDewarpingParams, settings.itemDewarpingParams));
    if (settings.contrastParams.enabled)
        result << QnAbstractImageFilterPtr(new QnContrastImageFilter(settings.contrastParams, tiledFilterExist));
    if (settings.rotation != 0)
        result << QnAbstractImageFilterPtr(new QnRotateImageFilter(settings.rotation));

    if (settings.timestampParams.enabled)
        result << QnAbstractImageFilterPtr(new QnTimeImageFilter(settings.layout, settings.timestampParams));

    //verifying that output resolution is supported by codec

    //adjusting output resolution if needed
    for (qreal prevResizeRatio = 1.0; prevResizeRatio > 0.07; )
    {
        //we can't be sure the way input image scale affects output, so adding a loop...

        const QSize resultResolution = applyFilterChain(result, srcResolution);
        if (resultResolution.width() <= resolutionLimit.width() &&
            resultResolution.height() <= resolutionLimit.height())
        {
            break;  //resolution is OK
        }

        qreal resizeRatio = 1.0;

        NX_ASSERT(resultResolution.width() > 0 && resultResolution.height() > 0);

        if (resultResolution.width() > resolutionLimit.width() &&
            resultResolution.width() > 0)
        {
            resizeRatio = (qreal)resolutionLimit.width() / resultResolution.width();
        }
        if (resultResolution.height() > resolutionLimit.height() &&
            resultResolution.height() > 0)
        {
            resizeRatio = std::min<qreal>(
                resizeRatio,
                (qreal)resolutionLimit.height() / resultResolution.height());
        }

        if (resizeRatio >= 1.0)
            break;  //done. resolution is OK

        if (resizeRatio >= prevResizeRatio - ASPECT_RATIO_COMPARISION_PRECISION)
            resizeRatio = prevResizeRatio * MIN_STEP_CHANGE_COEFF;   //this is needed for the loop to be finite
        prevResizeRatio = resizeRatio;

        //adjusting scale filter
        const QSize resizeToSize = QnCodecTranscoder::roundSize(
            QSize(srcResolution.width() * resizeRatio, srcResolution.height() * resizeRatio));

        auto scaleFilter = result.front().dynamicCast<QnScaleImageFilter>();
        if (!scaleFilter)
        {
            //adding scale filter
            scaleFilter.reset(new QnScaleImageFilter(resizeToSize));
            result.push_front(scaleFilter);
        }
        else
        {
            scaleFilter->setOutputImageSize(resizeToSize);
        }
    }

    return result;
}

#endif // ENABLE_DATA_PROVIDERS
