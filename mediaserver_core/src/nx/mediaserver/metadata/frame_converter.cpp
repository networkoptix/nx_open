#include "frame_converter.h"

#include <nx/utils/log/log.h>
#include <nx/sdk/metadata/pixel_format.h>
#include <nx/mediaserver/metadata/generic_uncompressed_video_frame.h>
#include <nx/mediaserver/metadata/yuv420_uncompressed_video_frame.h>

namespace nx {
namespace mediaserver {
namespace metadata {

using namespace nx::sdk::metadata;
using PixelFormat = UncompressedVideoFrame::PixelFormat;

/**
 * Converts rgb-like pixel formats. Asserts that pixelFormat is a supported RGB-based format.
 */
static AVPixelFormat rgbToAVPixelFormat(PixelFormat pixelFormat)
{
    switch (pixelFormat)
    {
        case PixelFormat::argb: return AV_PIX_FMT_ARGB;
        case PixelFormat::abgr: return AV_PIX_FMT_ABGR;
        case PixelFormat::rgba: return AV_PIX_FMT_RGBA;
        case PixelFormat::bgra: return AV_PIX_FMT_BGRA;
        case PixelFormat::rgb: return AV_PIX_FMT_RGB24;
        case PixelFormat::bgr: return AV_PIX_FMT_BGR24;

        default:
            NX_ASSERT(false, lm(
                "Unsupported nx::sdk::metadata::UncompressedVideoFrame::PixelFormat \"%1\" = %2")
                .args(pixelFormatToStdString(pixelFormat), (int) pixelFormat));
            return AV_PIX_FMT_NONE;
    }
}

static UncompressedVideoFrame* createUncompressedVideoFrameFromVideoDecoderOutput(
    const CLConstVideoDecoderOutputPtr& frame,
    PixelFormat pixelFormat)
{
    // TODO: Consider supporting other decoded video frame formats.
    const auto expectedFormat = AV_PIX_FMT_YUV420P;
    if (frame->format != expectedFormat)
    {
        NX_ERROR(typeid(FrameConverter)) << lm(
            "Decoded frame has AVPixelFormat %1 instead of %2; ignoring")
            .args(frame->format, expectedFormat);
        return nullptr;
    }

    if (pixelFormat == PixelFormat::yuv420)
        return new Yuv420UncompressedVideoFrame(frame);

    const AVPixelFormat avPixelFormat = rgbToAVPixelFormat(pixelFormat);

    std::vector<std::vector<char>> data(1);
    std::vector<int> lineSize(1);
    data[0] = frame->toRgb(&lineSize[0], avPixelFormat);

    return new GenericUncompressedVideoFrame(
        frame->pkt_dts, frame->width, frame->height, pixelFormat,
        std::move(data), std::move(lineSize));
}

DataPacket* FrameConverter::getDataPacket(boost::optional<PixelFormat> pixelFormat)
{
    if (!pixelFormat)
    {
        if (const auto compressedFrame = m_getCompressedFrame())
        {
            if (!m_compressedFrame)
            {
                m_compressedFrame.reset(new WrappingCompressedVideoPacket(compressedFrame));
                m_compressedFrame->releaseRef(); //< Make refCount = 1.
            }
            return m_compressedFrame.get();
        }

        warnOnce(m_compressedFrameWarningIssued,
            lm("Compressed frame requested but not received."));
        return nullptr;
    }

    if (const auto uncompressedFrame = m_getUncompressedFrame())
    {
        auto it = m_rgbFrames.find(pixelFormat.get());
        if (it == m_rgbFrames.cend())
        {
            auto insertResult = m_rgbFrames.emplace(
                pixelFormat.get(),
                nxpt::ScopedRef<UncompressedVideoFrame>(
                    createUncompressedVideoFrameFromVideoDecoderOutput(
                        uncompressedFrame, pixelFormat.get()),
                    /*increaseRef*/ false));
            it = insertResult.first;
        }
        return it->second.get();
    }

    warnOnce(m_uncompressedFrameWarningIssued,
        lm("Uncompressed frame requested but not received."));
    return nullptr;
}

void FrameConverter::warnOnce(bool* warningIssued, const QString& message)
{
    if (!*warningIssued)
    {
        *warningIssued = true;
        NX_WARNING(this) << message;
    }
    else
    {
        NX_VERBOSE(this) << message;
    }
}

} // namespace metadata
} // namespace mediaserver
} // namespace nx
