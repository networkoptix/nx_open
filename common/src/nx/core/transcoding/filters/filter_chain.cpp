#include "filter_chain.h"

#include <core/resource/media_resource.h>

#include <transcoding/filters/scale_image_filter.h>
#include <transcoding/filters/tiled_image_filter.h>
#include <transcoding/filters/crop_image_filter.h>
#include <transcoding/filters/fisheye_image_filter.h>
#include <transcoding/filters/contrast_image_filter.h>
#include <transcoding/filters/rotate_image_filter.h>
#include <transcoding/filters/time_image_filter.h>
#include <transcoding/filters/paint_image_filter.h>
#include <transcoding/filters/timestamp_filter.h>

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
    prepare(resource->getVideoLayout(),
        resource->getDewarpingParams(),
        srcFrameResolution,
        resolutionLimit);
}

void FilterChain::prepare(const QnConstResourceVideoLayoutPtr& videoLayout,
    const QnMediaDewarpingParams& mediaDewarpingParams,
    const QSize& srcFrameResolution,
    const QSize& resolutionLimit)
{
    NX_ASSERT(!isReady(), "Double initialization");
    NX_EXPECT(isTranscodingRequired(videoLayout));

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

    bool tiledFilterExist = false;
    if (videoLayout && videoLayout->channelCount() > 1)
    {
        push_back(QnAbstractImageFilterPtr(new QnTiledImageFilter(videoLayout)));
        tiledFilterExist = true;
    }

    if (!m_settings.zoomWindow.isEmpty() && !m_settings.dewarping.enabled)
        push_back(QnAbstractImageFilterPtr(new QnCropImageFilter(m_settings.zoomWindow)));

    if (m_settings.dewarping.enabled)
    {
        push_back(QnAbstractImageFilterPtr(new QnFisheyeImageFilter(
            mediaDewarpingParams, m_settings.dewarping)));
    }

    if (m_settings.enhancement.enabled)
    {
        push_back(QnAbstractImageFilterPtr(new QnContrastImageFilter(
            m_settings.enhancement, tiledFilterExist)));
    }

    if (m_settings.rotation != 0)
        push_back(QnAbstractImageFilterPtr(new QnRotateImageFilter(m_settings.rotation)));

    for (const auto overlaySettings: m_settings.overlays)
    {
        if (const auto imageFilterSetting = dynamic_cast<ImageOverlaySettings*>(
            overlaySettings.data()))
        {
            const auto paintFilter = new nx::transcoding::filters::PaintImageFilter();
            const auto filter = QnAbstractImageFilterPtr(paintFilter);
            paintFilter->setImage(imageFilterSetting->image,
                imageFilterSetting->offset,
                imageFilterSetting->alignment);

            push_back(filter);
        }
        else if (const auto timestampFilterSettings = dynamic_cast<TimestampOverlaySettings*>(
            overlaySettings.data()))
        {
            push_back(QnAbstractImageFilterPtr(new nx::transcoding::filters::TimestampFilter(
                *timestampFilterSettings)));
        }

    }

    //verifying that output resolution is supported by codec

    //adjusting output resolution if needed
    for (auto prevResizeRatio = 1.0; prevResizeRatio > 0.07; )
    {
        //we can't be sure the way input image scale affects output, so adding a loop...

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
            QSize(srcFrameResolution.width() * resizeRatio, srcFrameResolution.height() * resizeRatio));

        auto scaleFilter = front().dynamicCast<QnScaleImageFilter>();
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

    m_ready = true;
}

bool FilterChain::isTranscodingRequired(const QnConstResourceVideoLayoutPtr& videoLayout) const
{
    if (m_settings.aspectRatio.isValid())
        return true;

    if (videoLayout && videoLayout->channelCount() > 1)
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
    return isTranscodingRequired(resource
        ? resource->getVideoLayout()
        : QnConstResourceVideoLayoutPtr());
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

} // namespace transcoding
} // namespace core
} // namespace nx
