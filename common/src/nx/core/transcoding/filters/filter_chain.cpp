#include "filter_chain.h"

#if defined(ENABLE_DATA_PROVIDERS)

#include <core/resource/media_resource.h>

#include <transcoding/filters/scale_image_filter.h>
#include <transcoding/filters/tiled_image_filter.h>
#include <transcoding/filters/crop_image_filter.h>
#include <transcoding/filters/fisheye_image_filter.h>
#include <transcoding/filters/contrast_image_filter.h>
#include <transcoding/filters/rotate_image_filter.h>
#include <transcoding/filters/time_image_filter.h>
#include <nx/core/transcoding/filters/paint_image_filter.h>
#include <nx/core/transcoding/filters/timestamp_filter.h>

#include <transcoding/transcoder.h>

#include <nx/utils/log/assert.h>

namespace {

static constexpr float kMinStepChangeCoeff = 0.95f;
static constexpr float kAspectRatioComparisionPrecision = 0.01f;

} // namespace

namespace nx {
namespace core {
namespace transcoding {

const QSize FilterChain::kDefaultResolutionLimit(8192 - 16, 8192 - 16);

FilterChain::FilterChain(const Settings& settings):
    m_settings(settings)
{
}

void FilterChain::prepare(const QnMediaResourcePtr& resource,
    const QSize& srcFrameResolution,
    const QSize& resolutionLimit)
{
    NX_ASSERT(resource);
    if (!resource)
        return;

    const auto videoLayout = resource->getVideoLayout();

    NX_ASSERT(!isReady(), "Double initialization");
    //NX_EXPECT(isTranscodingRequired(videoLayout));

    prepareVideoArFilter(srcFrameResolution);

    const auto isPanoramicCamera = videoLayout && videoLayout->channelCount() > 1;
    if (isPanoramicCamera)
        push_back(QnAbstractImageFilterPtr(new QnTiledImageFilter(videoLayout)));

    prepareZoomWindowFilter();
    prepareDewarpingFilter(resource);
    prepareImageEnhancementFilter();
    prepareRotationFilter();
    prepareDownscaleFilter(srcFrameResolution, resolutionLimit);
    prepareOverlaysFilters();

    m_ready = true;
}


void FilterChain::prepareForImage(const QnMediaResourcePtr& resource,
    const QSize& fullImageResolution,
    const QSize& resolutionLimit)
{
    NX_ASSERT(resource);
    if (!resource)
        return;

    NX_ASSERT(!isReady(), "Double initialization");
    NX_EXPECT(isImageTranscodingRequired(fullImageResolution));

    prepareImageArFilter(resource, fullImageResolution);
    prepareZoomWindowFilter();
    prepareDewarpingFilter(resource);
    prepareImageEnhancementFilter();
    prepareRotationFilter();
    prepareDownscaleFilter(fullImageResolution, resolutionLimit);
    prepareOverlaysFilters();

    m_ready = true;
}

bool FilterChain::isImageTranscodingRequired(const QSize& fullImageResolution,
    const QSize& resolutionLimit) const
{
    return isTranscodingRequired()
        || fullImageResolution.width() > resolutionLimit.width()
        || fullImageResolution.height() > resolutionLimit.height();
}

bool FilterChain::isTranscodingRequired() const
{
    //TODO: #GDM #3.2 Remove when legacy will gone.
    if (!m_legacyFilters.empty())
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

    return !m_settings.overlays.isEmpty();
}

bool FilterChain::isTranscodingRequired(const QnMediaResourcePtr& resource) const
{
    NX_ASSERT(resource);

    QnConstResourceVideoLayoutPtr layout = resource ? resource->getVideoLayout() : QnConstResourceVideoLayoutPtr();
    if (layout && layout->channelCount() > 1)
        return true;
    return isTranscodingRequired();
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

CLVideoDecoderOutputPtr FilterChain::apply(const CLVideoDecoderOutputPtr& source) const
{
    auto result = source;
    for (auto filter: *this)
        result = filter->updateImage(result);
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

void FilterChain::prepareImageArFilter(const QnMediaResourcePtr& resource,
    const QSize& fullImageResolution)
{
    if (m_settings.aspectRatio.isValid())
    {
        QSize newSize;
        newSize.setHeight(fullImageResolution.height());

        auto aspectRatio = m_settings.aspectRatio.toFloat();
        if (const auto layout = resource->getVideoLayout())
            aspectRatio *= QnAspectRatio(layout->size()).toFloat();

        newSize.setWidth(fullImageResolution.height() * aspectRatio + 0.5);

        if (newSize != fullImageResolution)
        {
            push_back(QnAbstractImageFilterPtr(new QnScaleImageFilter(
                QnCodecTranscoder::roundSize(newSize))));
        }
    }
}

void FilterChain::prepareDewarpingFilter(const QnMediaResourcePtr& resource)
{
    if (m_settings.dewarping.enabled)
    {
        push_back(QnAbstractImageFilterPtr(new QnFisheyeImageFilter(
            resource->getDewarpingParams(), m_settings.dewarping)));
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
    for (const auto overlaySettings: m_settings.overlays)
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

    for (const auto legacyFilter: m_legacyFilters)
        push_back(legacyFilter);
}

void FilterChain::prepareDownscaleFilter(const QSize& srcFrameResolution,
    const QSize& resolutionLimit)
{
    // Verifying that output resolution is supported by codec. Adjusting if needed.
    for (auto prevResizeRatio = 1.0; prevResizeRatio > 0.07; )
    {
        // We can't be sure the way input image scale affects output, so adding a loop...
        const QSize resultResolution = apply(srcFrameResolution);
        if (resultResolution.width() <= resolutionLimit.width() &&
            resultResolution.height() <= resolutionLimit.height())
        {
            break;  //resolution is OK
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
        const auto resizeToSize = QnCodecTranscoder::roundSize(
            QSize(srcFrameResolution.width() * resizeRatio,
                srcFrameResolution.height() * resizeRatio));

        auto scaleFilter = !empty()
            ? front().dynamicCast<QnScaleImageFilter>()
            : QSharedPointer<QnScaleImageFilter>();

        if (!scaleFilter)
        {
            // Adding scale filter.
            scaleFilter.reset(new QnScaleImageFilter(resizeToSize));
            push_front(scaleFilter);
        }
        else
        {
            scaleFilter->setOutputImageSize(resizeToSize);
        }
    }
}

} // namespace transcoding
} // namespace core
} // namespace nx

#endif // ENABLE_DATA_PROVIDERS
