#include "uncompressed_video_frame.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/server/sdk_support/utils.h>

namespace nx::vms::server::analytics {

UncompressedVideoFrame::UncompressedVideoFrame(
    CLConstVideoDecoderOutputPtr clVideoDecoderOutput)
    :
    m_clVideoDecoderOutput(std::move(clVideoDecoderOutput))
{
    if (!NX_ASSERT(m_clVideoDecoderOutput))
        return;

    acceptAvFrame(m_clVideoDecoderOutput.get());
}

UncompressedVideoFrame::UncompressedVideoFrame(
    int width, int height, AVPixelFormat pixelFormat, int64_t dts)
    :
    m_ownedAvFrame(av_frame_alloc(),
        [](AVFrame* avFrame)
        {
            if (avFrame)
            {
                av_freep(&avFrame->data[0]);
                av_frame_free(&avFrame);
            }
        })
{
    if (!NX_ASSERT(m_ownedAvFrame)
        || !NX_ASSERT(width > 0, width)
        || !NX_ASSERT(height > 0, height)
        || !NX_ASSERT((int) pixelFormat >= 0, toString(pixelFormat)))
    {
        return;
    }

    m_ownedAvFrame->width = width;
    m_ownedAvFrame->height = height;
    m_ownedAvFrame->format = pixelFormat;
    m_ownedAvFrame->pkt_dts = dts;

    const int r = av_image_alloc(
        m_ownedAvFrame->data,
		m_ownedAvFrame->linesize,
		m_ownedAvFrame->width,
		m_ownedAvFrame->height,
		(AVPixelFormat) m_ownedAvFrame->format,
		CL_MEDIA_ALIGNMENT);

    if (!NX_ASSERT(r > 0, lm("av_image_alloc() failed for pixel format %1").arg(pixelFormat)))
        return;

    acceptAvFrame(m_ownedAvFrame.get());
}

/**
 * Ffmpeg does not store the number of planes in a frame explicitly, thus, deducing it. On error,
 * fails an assertion and returns 0.
 */
static int deducePlaneCount(
    const AVPixFmtDescriptor* avPixFmtDescriptor, AVPixelFormat avPixelFormat)
{
    // As per the doc for AVComponentDescriptor (AVPixFmtDescriptor::comp), each pixel has 1 to 4
    // so-called components (e.g. R, G, B, Y, U, V or A), each component resides in a certain plane
    // (0..3), and a plane can host more than one component (e.g. for RGB24 the only plane #0 hosts
    // all 3 components).

    // Find the max plane index for all components.
    int maxPlane = -1;
    for (int component = 0; component < avPixFmtDescriptor->nb_components; ++component)
    {
        const int componentPlane = avPixFmtDescriptor->comp[component].plane;
        if (!NX_ASSERT(componentPlane >= 0 && componentPlane < AV_NUM_DATA_POINTERS,
            lm("AVPixFmtDescriptor reports plane %1 for component %2 of pixel format %3").args(
                componentPlane, component, avPixelFormat)))
        {
            return 0;
        }

        if (componentPlane > maxPlane)
            maxPlane = componentPlane;
    }

    return maxPlane + 1;
}

/**
 * Ffmpeg does not store the number of lines per plane in a frame explicitly, thus, deducing it.
 */
static int deducePlaneLineCount(
    int plane, const AVPixFmtDescriptor* avPixFmtDescriptor, const AVFrame* avFrame)
{
    // See the doc for AVComponentDescriptor (AVPixFmtDescriptor::comp).

    if (avPixFmtDescriptor->nb_components >= 3
        && !(avFrame->flags & AV_PIX_FMT_FLAG_RGB))
	{
        // Plane 0 is luma, planes 1 and 2 are chroma.
        if (plane == 1 || plane == 2) //< A chroma plane, can have a reduced vertical resolution.
            return avFrame->height >> avPixFmtDescriptor->log2_chroma_h;
	}

    // The plane is non-chroma (presumably, Y, RGB or A) - the plane's height equals the frame's.
	return avFrame->height;
}

/**
 * Called at the end of constructors. Assigns m_avFrame with avFrame on success.
 */
void UncompressedVideoFrame::acceptAvFrame(const AVFrame* avFrame)
{
    if (!NX_ASSERT(avFrame->width >= 1, QString::number(avFrame->width))
        || !NX_ASSERT(avFrame->height >= 1, QString::number(avFrame->height)))
    {
        return;
    }

    const auto avPixelFormat = (AVPixelFormat) avFrame->format;
    const std::optional<PixelFormat> sdkPixelFormat =
        nx::vms::server::sdk_support::avPixelFormatToSdk(avPixelFormat);
    if (!NX_ASSERT(sdkPixelFormat, toString(avPixelFormat)))
        return;
    m_pixelFormat = sdkPixelFormat.value();

    m_avPixFmtDescriptor = av_pix_fmt_desc_get(avPixelFormat);
    if (!NX_ASSERT(m_avPixFmtDescriptor, toString(avPixelFormat)))
        return;

    const int componentCount = m_avPixFmtDescriptor->nb_components;
    if (!NX_ASSERT(componentCount >= 1 && componentCount <= 4,
        lm("AVPixFmtDescriptor reports invalid nb_components %1 for pixel format %2").args(
            componentCount, avPixelFormat)))
    {
        return;
    }

    m_dataSize.resize(deducePlaneCount(m_avPixFmtDescriptor, avPixelFormat));
    if (m_dataSize.empty())
        return; //< An assertion has already failed.
    for (int plane = 0; plane < (int) m_dataSize.size(); ++plane)
    {
        if (!NX_ASSERT(avFrame->data[plane],
                lm("Null data for plane %1, pixel format %2").args(plane, avPixelFormat))
            || !NX_ASSERT(avFrame->linesize[plane] > 0,
                lm("Invalid linesize %1 for plane %2, pixel format %3").args(
                    avFrame->linesize[plane], plane, avPixelFormat)))
        {
            return;
        }

        m_dataSize[plane] = avFrame->linesize[plane]
            * deducePlaneLineCount(plane, m_avPixFmtDescriptor, avFrame);
    }

    m_avFrame = avFrame;
}

int64_t UncompressedVideoFrame::timestampUs() const
{
    if (!assertValid(__func__))
        return 0;

    // IUncompressedVideoFrame requires the timestamp to be either valid or zero.
    return m_avFrame->pkt_dts < 0 ? 0 : m_avFrame->pkt_dts;
}

int UncompressedVideoFrame::width() const
{
    return !assertValid(__func__) ? 0 : m_avFrame->width;
}

int UncompressedVideoFrame::height() const
{
    return !assertValid(__func__) ? 0 : m_avFrame->height;
}

UncompressedVideoFrame::PixelAspectRatio UncompressedVideoFrame::pixelAspectRatio() const
{
    if (!assertValid(__func__))
        return {0, 1};

    return {m_avFrame->sample_aspect_ratio.num, m_avFrame->sample_aspect_ratio.den};
}

UncompressedVideoFrame::PixelFormat UncompressedVideoFrame::pixelFormat() const
{
    return !assertValid(__func__) ? ((PixelFormat) 0) : m_pixelFormat;
}

int UncompressedVideoFrame::planeCount() const
{
    return !assertValid(__func__) ? 0 : (int) m_dataSize.size();
}

int UncompressedVideoFrame::dataSize(int plane) const
{
    if (!assertPlaneValid(plane, __func__)
        || !NX_ASSERT(m_dataSize[plane] >= 0,
            lm("Invalid dataSize %1 for plane %2").args(m_dataSize[plane], plane)))
    {
        return 0;
    }

    return m_dataSize[plane];
}

const char* UncompressedVideoFrame::data(int plane) const
{
    if (!assertPlaneValid(plane, __func__)
        || !NX_ASSERT(m_avFrame->data[plane], lm("Null data for plane %1").arg(plane)))
    {
        return nullptr;
    }

    return (const char*) m_avFrame->data[plane];
}

int UncompressedVideoFrame::lineSize(int plane) const
{
    if (!assertPlaneValid(plane, __func__))
        return 0;

    return m_avFrame->linesize[plane];
}

bool UncompressedVideoFrame::assertValid(const char* func) const
{
    return NX_ASSERT(m_avFrame, lm("%1()").arg(func));
}

bool UncompressedVideoFrame::assertPlaneValid(int plane, const char* func) const
{
    if (!assertValid(func))
        return false;

    return NX_ASSERT(plane >= 0 && plane < m_dataSize.size(),
        lm("%1(): Requested plane %2 of %3").args(func, plane, m_dataSize.size()));
}

} // namespace nx::vms::server::analytics
