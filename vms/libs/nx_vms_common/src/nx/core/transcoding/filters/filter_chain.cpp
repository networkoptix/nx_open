// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "filter_chain.h"

#include <core/resource/media_resource.h>
#include <nx/core/transcoding/filters/paint_image_filter.h>
#include <nx/core/transcoding/filters/timestamp_filter.h>
#include <nx/core/transcoding/filters/watermark_filter.h>
#include <nx/utils/log/assert.h>
#include <transcoding/filters/contrast_image_filter.h>
#include <transcoding/filters/crop_image_filter.h>
#include <transcoding/filters/dewarping_image_filter.h>
#include <nx/core/transcoding/filters/pixelation_image_filter.h>
#include <transcoding/filters/rotate_image_filter.h>
#include <transcoding/filters/scale_image_filter.h>
#include <transcoding/filters/tiled_image_filter.h>
#include <transcoding/transcoder.h>

namespace {

static constexpr float kMinStepChangeCoeff = 0.95f;
static constexpr float kAspectRatioComparisionPrecision = 0.01f;

} // namespace

namespace nx {
namespace core {
namespace transcoding {

const QSize FilterChain::kDefaultResolutionLimit(8192 - 16, 8192 - 16);

FilterChain::FilterChain(const Settings& settings,
    nx::vms::api::dewarping::MediaData dewarpingParams,
    QnConstResourceVideoLayoutPtr layout)
    :
    m_settings(settings),
    m_dewarpingParams(dewarpingParams),
    m_layout(layout)
{
}

void FilterChain::prepare(const QSize& srcFrameResolution, const QSize& resolutionLimit)
{
    NX_ASSERT(!isReady(), "Double initialization");

    prepareVideoArFilter(srcFrameResolution);

    const auto isPanoramicCamera = m_layout && m_layout->channelCount() > 1;
    if (isPanoramicCamera)
        push_back(QnAbstractImageFilterPtr(new QnTiledImageFilter(m_layout)));

    createPixelationImageFilter();
    prepareZoomWindowFilter();
    prepareDewarpingFilter();
    prepareImageEnhancementFilter();
    prepareRotationFilter();
    prepareDownscaleFilter(srcFrameResolution, resolutionLimit);
    prepareOverlaysFilters();
    prepareWatermarkFilter();

    m_ready = true;
}

void FilterChain::prepareForImage(const QSize& fullImageResolution, const QSize& resolutionLimit)
{
    NX_ASSERT(!isReady(), "Double initialization");

    if (!isImageTranscodingRequired(fullImageResolution, resolutionLimit))
        return;

    createPixelationImageFilter();
    prepareImageArFilter(fullImageResolution);
    prepareZoomWindowFilter();
    prepareDewarpingFilter();
    prepareImageEnhancementFilter();
    prepareRotationFilter();
    prepareDownscaleFilter(fullImageResolution, resolutionLimit);
    prepareOverlaysFilters();
    prepareWatermarkFilter();

    NX_ASSERT(!isEmpty());

    m_ready = true;
}

bool FilterChain::isImageTranscodingRequired(const QSize& fullImageResolution,
    const QSize& resolutionLimit) const
{
    return isTranscodingRequired(/*concernTiling*/ false)
        || fullImageResolution.width() > resolutionLimit.width()
        || fullImageResolution.height() > resolutionLimit.height();
}

bool FilterChain::isTranscodingRequired(bool concernTiling) const
{
    if (concernTiling && m_layout && m_layout->channelCount() > 1)
        return true;

    //TODO: #sivanov Remove when legacy is gone.
    if (!m_legacyFilters.empty())
        return true;

    if (m_settings.forceTranscoding)
        return true;

    if (m_settings.aspectRatio.isValid())
        return true;

    if (!m_settings.zoomWindow.isEmpty())
        return true;

    if (m_settings.dewarping.enabled)
        return true;

    if (m_settings.enhancement.enabled)
        return true;

    if (m_settings.rotation != 0)
        return true;

    if (m_settings.watermark.visible())
        return true;

    return !m_settings.overlays.isEmpty();
}

bool FilterChain::isDownscaleRequired(const QSize& srcResolution) const
{
    NX_ASSERT(isReady());

    const auto resultResolution = apply(srcResolution);
    return resultResolution.width() > kDefaultResolutionLimit.width() ||
        resultResolution.height() > kDefaultResolutionLimit.height();
}

bool FilterChain::isReady() const
{
    return m_ready;
}

void FilterChain::reset()
{
    clear();
    m_ready = false;
}

QSize FilterChain::apply(const QSize& resolution) const
{
    auto result = resolution;
    for (auto filter: *this)
        result = filter->updatedResolution(result);
    return result;
}

CLVideoDecoderOutputPtr FilterChain::apply(
    const CLVideoDecoderOutputPtr& source,
    const QnAbstractCompressedMetadataPtr& metadata) const
{
    if (empty() || !source)
        return source;

    // Make a deep copy of the source frame since modifying a frame received from a decoder can
    // affect further decoding process.
    CLVideoDecoderOutputPtr result(new CLVideoDecoderOutput());
    result->copyFrom(source.get());

    for (auto filter: *this)
        result = filter->updateImage(result, metadata);

    return result;
}

void FilterChain::addLegacyFilter(QnAbstractImageFilterPtr filter)
{
    m_legacyFilters.append(filter);
}

void FilterChain::prepareVideoArFilter(const QSize& srcFrameResolution)
{
    if (m_settings.aspectRatio.isValid())
    {
        QSize newSize;
        newSize.setHeight(srcFrameResolution.height());
        newSize.setWidth(srcFrameResolution.height() * m_settings.aspectRatio.toFloat() + 0.5);
        if (newSize != srcFrameResolution)
        {
            push_back(QnAbstractImageFilterPtr(new QnScaleImageFilter(
                QnCodecTranscoder::roundSize(newSize))));
        }
    }
}

void FilterChain::prepareImageArFilter(const QSize& fullImageResolution)
{
    if (m_settings.aspectRatio.isValid())
    {
        QSize newSize;
        newSize.setHeight(fullImageResolution.height());

        auto aspectRatio = m_settings.aspectRatio.toFloat();
        if (m_layout)
            aspectRatio *= QnAspectRatio(m_layout->size()).toFloat();

        newSize.setWidth(fullImageResolution.height() * aspectRatio + 0.5);

        if (newSize != fullImageResolution)
        {
            push_back(QnAbstractImageFilterPtr(new QnScaleImageFilter(
                QnCodecTranscoder::roundSize(newSize))));
        }
    }
}

void FilterChain::prepareDewarpingFilter()
{
    if (m_settings.dewarping.enabled)
    {
        push_back(QnAbstractImageFilterPtr(new QnDewarpingImageFilter(
            m_dewarpingParams, m_settings.dewarping)));
    }
}

void FilterChain::prepareZoomWindowFilter()
{
    if (!m_settings.zoomWindow.isEmpty() && !m_settings.dewarping.enabled)
        push_back(QnAbstractImageFilterPtr(new QnCropImageFilter(m_settings.zoomWindow)));
}

void FilterChain::prepareImageEnhancementFilter()
{
    if (m_settings.enhancement.enabled)
        push_back(QnAbstractImageFilterPtr(new QnContrastImageFilter(m_settings.enhancement)));
}

void FilterChain::prepareRotationFilter()
{
    if (m_settings.rotation != 0)
        push_back(QnAbstractImageFilterPtr(new QnRotateImageFilter(m_settings.rotation)));
}

void FilterChain::prepareOverlaysFilters()
{
    for (const auto& overlaySettings: m_settings.overlays)
    {
        if (const auto imageFilterSetting = dynamic_cast<ImageOverlaySettings*>(
            overlaySettings.data()))
        {
            const auto paintFilter = new PaintImageFilter();
            paintFilter->setImage(imageFilterSetting->image,
                imageFilterSetting->offset,
                imageFilterSetting->alignment);

            push_back(QnAbstractImageFilterPtr(paintFilter));
        }
        else if (const auto timestampFilterSettings = dynamic_cast<TimestampOverlaySettings*>(
            overlaySettings.data()))
        {
            push_back(QnAbstractImageFilterPtr(new TimestampFilter(*timestampFilterSettings)));
        }
    }

    for (const auto& legacyFilter: m_legacyFilters)
        push_back(legacyFilter);
}

void FilterChain::prepareWatermarkFilter()
{
    if (m_settings.watermark.visible())
        push_back(QnAbstractImageFilterPtr(new WatermarkImageFilter(m_settings.watermark)));
}

void FilterChain::createScaleImageFilter(const QSize& dstSize)
{
    auto scaleFilter = !empty()
        ? front().dynamicCast<QnScaleImageFilter>()
        : QSharedPointer<QnScaleImageFilter>();

    if (!scaleFilter)
    {
        // Adding scale filter.
        scaleFilter.reset(new QnScaleImageFilter(dstSize));
        push_front(scaleFilter);
    }
    else
    {
        scaleFilter->setOutputImageSize(dstSize);
    }
}

void FilterChain::createPixelationImageFilter()
{
    if (m_settings.pixelationSettings)
    {
        push_front(QnAbstractImageFilterPtr(
            new PixelationImageFilter(*m_settings.pixelationSettings)));
    }
}

void FilterChain::prepareDownscaleFilter(const QSize& srcFrameResolution,
    const QSize& resolutionLimit)
{
    // Verifying that output resolution is supported by codec. Adjusting if needed.
    for (auto prevResizeRatio = 1.0; prevResizeRatio > 0.07; )
    {
        // We can't be sure the way input image scale affects output, so adding a loop...
        const QSize resultResolution = QnCodecTranscoder::roundSize(apply(srcFrameResolution));
        if (resultResolution.width() <= resolutionLimit.width() &&
            resultResolution.height() <= resolutionLimit.height())
        {
            if (resultResolution != srcFrameResolution && empty())
                createScaleImageFilter(resultResolution);
            return;  //resolution is OK
        }

        auto resizeRatio = 1.0;

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

        // This is needed for the loop to be finite.
        if (resizeRatio >= prevResizeRatio - kAspectRatioComparisionPrecision)
            resizeRatio = prevResizeRatio * kMinStepChangeCoeff;
        prevResizeRatio = resizeRatio;

        // Adjusting scale filter.
        // Due to size ceiled, some more iteration can be needed.
        // TODO refactor this -> use floor instead ceil or select new one by subtracting of alignment
        // value?
        const auto resizeToSize = QnCodecTranscoder::roundSize(
            QSize(srcFrameResolution.width() * resizeRatio,
                srcFrameResolution.height() * resizeRatio));

        createScaleImageFilter(resizeToSize);
    }
}

} // namespace transcoding
} // namespace core
} // namespace nx
