// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "filter_chain.h"

#include <core/resource/media_resource.h>
#include <nx/core/transcoding/filters/motion_filter.h>
#include <nx/core/transcoding/filters/object_info_filter.h>
#include <nx/core/transcoding/filters/paint_image_filter.h>
#include <nx/core/transcoding/filters/pixelation_image_filter.h>
#include <nx/core/transcoding/filters/timestamp_filter.h>
#include <nx/core/transcoding/filters/watermark_filter.h>
#include <nx/utils/log/assert.h>
#include <transcoding/filters/contrast_image_filter.h>
#include <transcoding/filters/crop_image_filter.h>
#include <transcoding/filters/dewarping_image_filter.h>
#include <transcoding/filters/rotate_image_filter.h>
#include <transcoding/filters/scale_image_filter.h>
#include <transcoding/filters/tiled_image_filter.h>
#include <transcoding/transcoding_utils.h>

namespace {

static constexpr float kMinStepChangeCoeff = 0.95f;
static constexpr float kAspectRatioComparisionPrecision = 0.01f;

} // namespace

namespace nx::core::transcoding {

const QSize FilterChain::kDefaultResolutionLimit(8192 - 16, 8192 - 16);

FilterChain::FilterChain(const Settings& settings):
    m_settings(settings)
{
}

void FilterChain::prepare(const QSize& srcFrameResolution, const QSize& resolutionLimit)
{
    NX_ASSERT(!isReady(), "Double initialization");

    if (!m_settings.resolution)
        prepareVideoArFilter(srcFrameResolution);

    const auto isPanoramicCamera = m_settings.layout && m_settings.layout->channelCount() > 1;
    if (isPanoramicCamera)
        m_filters.push_back(QnAbstractImageFilterPtr(new QnTiledImageFilter(m_settings.layout)));

    createPixelationImageFilter();
    createObjectInfoFilter();
    createMotionFilter();

    prepareZoomWindowFilter();
    prepareDewarpingFilter();
    prepareImageEnhancementFilter();
    prepareRotationFilter();
    if (!m_settings.resolution)
        prepareDownscaleFilter(srcFrameResolution, resolutionLimit);
    prepareOverlaysFilters();
    prepareWatermarkFilter();

    if (m_settings.resolution)
    {
        m_filters.push_back(
            QnAbstractImageFilterPtr(new QnScaleImageFilter(*m_settings.resolution)));
    }

    m_ready = true;
}

void FilterChain::prepareForImage(const QSize& fullImageResolution)
{
    NX_ASSERT(!isReady(), "Double initialization");

    // TODO: here there are no settings.pixelation enabled
    if (!isImageTranscodingRequired(fullImageResolution, kDefaultResolutionLimit))
        return;

    createPixelationImageFilter();
    createObjectInfoFilter();
    createMotionFilter();

    prepareImageArFilter(fullImageResolution);
    prepareZoomWindowFilter();
    prepareDewarpingFilter();
    prepareImageEnhancementFilter();
    prepareRotationFilter();
    prepareDownscaleFilter(fullImageResolution, kDefaultResolutionLimit);
    prepareOverlaysFilters();
    prepareWatermarkFilter();

    NX_ASSERT(!m_filters.empty());

    m_ready = true;
}

bool FilterChain::isImageTranscodingRequired(const QSize& fullImageResolution,
    const QSize& resolutionLimit) const
{
    return m_settings.isTranscodingRequired(/*concernTiling*/ false)
        || fullImageResolution.width() > resolutionLimit.width()
        || fullImageResolution.height() > resolutionLimit.height();
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
    m_filters.clear();
    m_metadataFilters.clear();
    m_ready = false;
}

void FilterChain::setMetadata(
    const std::list<QnConstAbstractCompressedMetadataPtr>& objectsData,
    const std::list<QnConstAbstractCompressedMetadataPtr>& motionData)
{
    for (auto filter: m_filters)
    {
        if (auto pixelationFilter = filter.dynamicCast<PixelationImageFilter>())
            pixelationFilter->setMetadata(objectsData);
    }
    for (auto filter: m_metadataFilters)
    {
        if (auto objectInfoFilter = filter.dynamicCast<ObjectInfoFilter>())
            objectInfoFilter->setMetadata(objectsData);

        if (auto motionFilter = filter.dynamicCast<MotionFilter>())
            motionFilter->setMetadata(motionData);
    }
}

QSize FilterChain::apply(const QSize& resolution) const
{
    auto result = resolution;
    for (auto filter: m_metadataFilters)
        result = filter->updatedResolution(result);
    for (auto filter: m_filters)
        result = filter->updatedResolution(result);
    return result;
}

CLVideoDecoderOutputPtr FilterChain::apply(const CLVideoDecoderOutputPtr& source) const
{
    if ((m_filters.empty() && m_metadataFilters.empty()) || !source)
        return source;

    CLVideoDecoderOutputPtr result;
    if (!m_metadataFilters.empty())
    {
        auto image = source->toImage();
        for (auto filter: m_metadataFilters)
            filter->updateImage(image);

        // Force color scheme to translate colors used to draw metadata precisely.
        // Otherwise, them are changed into blurred and unclear colors with incorrect opacity
        // level.
        result = CLVideoDecoderOutputPtr(new CLVideoDecoderOutput(
            image, SWS_CS_ITU601));
        result->assignMiscData(source.get());
    }
    else
    {
        // Make a deep copy of the source frame since modifying a frame received from a decoder can
        // affect further decoding process.
        result = CLVideoDecoderOutputPtr(new CLVideoDecoderOutput());
        result->copyFrom(source.get());
    }

    for (auto filter: m_filters)
        result = filter->updateImage(result);

    return result;
}

void FilterChain::addLegacyFilter(QnAbstractImageFilterPtr filter)
{
    m_legacyFilters.push_back(filter);
}

bool FilterChain::empty()
{
    return m_filters.empty() && m_legacyFilters.empty();
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
            m_filters.push_back(QnAbstractImageFilterPtr(new QnScaleImageFilter(
                nx::transcoding::roundSize(newSize))));
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
        if (m_settings.layout)
            aspectRatio *= QnAspectRatio(m_settings.layout->size()).toFloat();

        newSize.setWidth(fullImageResolution.height() * aspectRatio + 0.5);

        if (newSize != fullImageResolution)
        {
            m_filters.push_back(QnAbstractImageFilterPtr(new QnScaleImageFilter(
                nx::transcoding::roundSize(newSize))));
        }
    }
}

void FilterChain::prepareDewarpingFilter()
{
    if (m_settings.dewarping.enabled)
    {
        m_filters.push_back(QnAbstractImageFilterPtr(new QnDewarpingImageFilter(
            m_settings.dewarpingMedia, m_settings.dewarping)));
    }
}

void FilterChain::prepareZoomWindowFilter()
{
    if (!m_settings.zoomWindow.isEmpty() && !m_settings.dewarping.enabled)
    {
        m_filters.push_back(QnAbstractImageFilterPtr(
            new QnCropImageFilter(m_settings.zoomWindow, m_settings.zoomWindowAlignSize)));
    }
}

void FilterChain::prepareImageEnhancementFilter()
{
    if (m_settings.enhancement.enabled)
        m_filters.push_back(QnAbstractImageFilterPtr(new QnContrastImageFilter(m_settings.enhancement)));
}

void FilterChain::prepareRotationFilter()
{
    if (m_settings.rotation != 0)
        m_filters.push_back(QnAbstractImageFilterPtr(new QnRotateImageFilter(m_settings.rotation)));
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

            m_filters.push_back(QnAbstractImageFilterPtr(paintFilter));
        }
        else if (const auto timestampFilterSettings = dynamic_cast<TimestampOverlaySettings*>(
            overlaySettings.data()))
        {
            m_filters.push_back(QnAbstractImageFilterPtr(new TimestampFilter(*timestampFilterSettings)));
        }
    }

    for (const auto& legacyFilter: m_legacyFilters)
        m_filters.push_back(legacyFilter);
}

void FilterChain::prepareWatermarkFilter()
{
    if (m_settings.watermark.visible())
        m_filters.push_back(QnAbstractImageFilterPtr(new WatermarkImageFilter(m_settings.watermark)));
}

void FilterChain::createScaleImageFilter(const QSize& dstSize)
{
    auto scaleFilter = !m_filters.empty()
        ? m_filters.front().dynamicCast<QnScaleImageFilter>()
        : QSharedPointer<QnScaleImageFilter>();

    if (!scaleFilter)
    {
        // Adding scale filter.
        scaleFilter.reset(new QnScaleImageFilter(dstSize));
        m_filters.push_front(scaleFilter);
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
        m_filters.push_front(QnAbstractImageFilterPtr(
            new PixelationImageFilter(*m_settings.pixelationSettings)));
    }
}

void FilterChain::createMotionFilter()
{
    const auto& motionSettings = m_settings.motionExportSettings;
    if (motionSettings)
        m_metadataFilters.push_back(AbstractMetadataFilterPtr(new MotionFilter(*motionSettings)));
}

void FilterChain::createObjectInfoFilter()
{
    const auto& oESettings = m_settings.objectExportSettings;
    if (!oESettings || !oESettings->taxonomyState || oESettings->typeSettings.empty())
        return;

    m_metadataFilters.push_back(AbstractMetadataFilterPtr(new ObjectInfoFilter(*oESettings)));
}

void FilterChain::prepareDownscaleFilter(const QSize& srcFrameResolution,
    const QSize& resolutionLimit)
{
    // Verifying that output resolution is supported by codec. Adjusting if needed.
    for (auto prevResizeRatio = 1.0; prevResizeRatio > 0.07; )
    {
        // We can't be sure the way input image scale affects output, so adding a loop...
        const QSize resultResolution = nx::transcoding::roundSize(apply(srcFrameResolution));
        if (resultResolution.width() <= resolutionLimit.width() &&
            resultResolution.height() <= resolutionLimit.height())
        {
            if (resultResolution != srcFrameResolution && m_filters.empty())
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
        const auto resizeToSize = nx::transcoding::roundSize(
            QSize(srcFrameResolution.width() * resizeRatio,
                srcFrameResolution.height() * resizeRatio));

        createScaleImageFilter(resizeToSize);
    }
}

} // namespace nx::core::transcoding
