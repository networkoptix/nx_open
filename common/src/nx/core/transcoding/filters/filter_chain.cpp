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

namespace
{

static const float kMinStepChangeCoeff = 0.95f;
static const float kAspectRatioComparisionPrecision = 0.01f;

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
    const QSize& srcResolution,
    const QSize& resolutionLimit)
{
    NX_ASSERT(!isReady(), "Double initialization");
    NX_EXPECT(isTranscodingRequired(resource));

    if (m_settings.aspectRatio.isValid())
    {
        QSize newSize;
        newSize.setHeight(srcResolution.height());
        newSize.setWidth(srcResolution.height() * m_settings.aspectRatio.toFloat() + 0.5);
        if (newSize != srcResolution)
        {
            push_back(QnAbstractImageFilterPtr(new QnScaleImageFilter(
                QnCodecTranscoder::roundSize(newSize))));
        }
    }

    bool tiledFilterExist = false;
    const auto layout = resource->getVideoLayout();
    if (layout && layout->channelCount() > 1)
    {
        push_back(QnAbstractImageFilterPtr(new QnTiledImageFilter(layout)));
        tiledFilterExist = true;
    }

    if (!m_settings.zoomWindow.isEmpty() && !m_settings.dewarping.enabled)
        push_back(QnAbstractImageFilterPtr(new QnCropImageFilter(m_settings.zoomWindow)));

    if (m_settings.dewarping.enabled)
    {
        push_back(QnAbstractImageFilterPtr(new QnFisheyeImageFilter(
            resource->getDewarpingParams(), m_settings.dewarping)));
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
        switch (overlaySettings->type())
        {
            case OverlaySettings::Type::image:
            {
                const auto imageFilterSetting =
                    static_cast<ImageOverlaySettings*>(overlaySettings.data());
                const auto paintFilter = new nx::transcoding::filters::PaintImageFilter();
                const auto filter = QnAbstractImageFilterPtr(paintFilter);
                paintFilter->setImage(imageFilterSetting->image,
                    imageFilterSetting->offset,
                    imageFilterSetting->anchors);
                push_back(filter);
                break;
            }

            case OverlaySettings::Type::timestamp:
            {
                const auto timestampFilterSettings =
                    static_cast<TimestampOverlaySettings*>(overlaySettings.data());
                push_back(QnAbstractImageFilterPtr(new nx::transcoding::filters::TimestampFilter(
                    *timestampFilterSettings)));
                break;
            }

            default:
                NX_ASSERT(false, Q_FUNC_INFO, "Unknown overlay type");
        }
    }

    //verifying that output resolution is supported by codec

    //adjusting output resolution if needed
    for (auto prevResizeRatio = 1.0; prevResizeRatio > 0.07; )
    {
        //we can't be sure the way input image scale affects output, so adding a loop...

        const QSize resultResolution = apply(srcResolution);
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
            QSize(srcResolution.width() * resizeRatio, srcResolution.height() * resizeRatio));

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

bool FilterChain::isTranscodingRequired(const QnMediaResourcePtr& resource) const
{
    if (m_settings.aspectRatio.isValid())
        return true;

    const auto layout = resource->getVideoLayout();
    if (layout && layout->channelCount() > 1)
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
