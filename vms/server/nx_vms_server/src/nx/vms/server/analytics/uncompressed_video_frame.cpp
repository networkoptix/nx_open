#include "uncompressed_video_frame.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/server/sdk_support/utils.h>

namespace nx::vms::server::analytics {

UncompressedVideoFrame::UncompressedVideoFrame(std::shared_ptr<const AVFrame> avFrame):
    m_avFrame(std::move(avFrame))
{
    if (!NX_ASSERT(m_avFrame) || !NX_ASSERT(std::get_deleter<void*>(m_avFrame)))
        return;

    if (!NX_ASSERT(m_avFrame->width >= 1) || !NX_ASSERT(m_avFrame->height >= 1))
        return;

    const auto avPixelFormat = (AVPixelFormat) m_avFrame->format;

    const auto pixelFormat = nx::vms::server::sdk_support::avPixelFormatToSdk(avPixelFormat);
    if (!NX_ASSERT(pixelFormat))
        return;
    m_pixelFormat = pixelFormat.value();

    m_avPixFmtDescriptor = av_pix_fmt_desc_get(avPixelFormat);
    if (!NX_ASSERT(m_avPixFmtDescriptor, toString(avPixelFormat))
        || !NX_ASSERT(m_avPixFmtDescriptor->nb_components >= 1, toString(avPixelFormat))
        || !NX_ASSERT(m_avPixFmtDescriptor->nb_components <= 4, toString(avPixelFormat)))
    {
        return;
    }

    m_dataSize.resize(m_avPixFmtDescriptor->nb_components);
    for (int plane = 0; plane < m_avPixFmtDescriptor->nb_components; ++plane)
    {
        if (!NX_ASSERT(m_avFrame->data[plane]) || !NX_ASSERT(m_avFrame->linesize[plane] > 0))
            return;

        m_dataSize[plane] = m_avFrame->linesize[plane] * planeLineCount(plane);
    }

    m_valid = true;
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
    return !assertValid(__func__) ? 0 : m_avPixFmtDescriptor->nb_components;
}

int UncompressedVideoFrame::dataSize(int plane) const
{
    if (!assertPlaneValid(plane, __func__))
        return 0;

    return m_dataSize[plane];
}

const char* UncompressedVideoFrame::data(int plane) const
{
    if (!assertPlaneValid(plane, __func__))
        return nullptr;

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
    return NX_ASSERT(m_valid, lm("%1()").arg(func));
}

bool UncompressedVideoFrame::assertPlaneValid(int plane, const char* func) const
{
    if (!assertValid(func))
        return false;

    return NX_ASSERT(plane >= 0 && plane < m_avPixFmtDescriptor->nb_components,
        lm("%1(): Requested plane %2 of %3").args(
            func, plane, m_avPixFmtDescriptor->nb_components));
}

int UncompressedVideoFrame::planeLineCount(int plane) const
{
    // Algorithm is based on the doc for AVFrame::comp field.

    if (m_avPixFmtDescriptor->nb_components <= 2 || (m_avFrame->flags & AV_PIX_FMT_FLAG_RGB))
        return m_avFrame->height; //< The frame planes are Y or Y/A or R/G/B - all of the same size.

    // The frame has 3 or 4 planes; the plane 4 (if present) is A.

    if (plane == 0 || plane == 4)
        return m_avFrame->height; //< The plane is Y or A.

    // The plane is a chroma plane: U or V.
    return m_avFrame->height >> m_avPixFmtDescriptor->log2_chroma_h;
}

} // namespace nx::vms::server::analytics
