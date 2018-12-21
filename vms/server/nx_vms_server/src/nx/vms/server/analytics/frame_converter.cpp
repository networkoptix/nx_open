#include "frame_converter.h"

#include <nx/utils/log/log.h>
#include <nx/sdk/common/ptr.h>
#include <nx/sdk/analytics/common/pixel_format.h>
#include <nx/vms/server/analytics/generic_uncompressed_video_frame.h>
#include <nx/vms/server/analytics/yuv420_uncompressed_video_frame.h>

namespace nx {
namespace vms::server {
namespace analytics {

using namespace nx::sdk::analytics;
using PixelFormat = IUncompressedVideoFrame::PixelFormat;

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
            NX_ASSERT(false, lm("Unsupported PixelFormat \"%1\" = %2").args(
                nx::sdk::analytics::common::pixelFormatToStdString(pixelFormat),
                (int) pixelFormat));
            return AV_PIX_FMT_NONE;
    }
}

static IUncompressedVideoFrame* createUncompressedVideoFrameFromVideoDecoderOutput(
    const CLConstVideoDecoderOutputPtr& frame,
    PixelFormat pixelFormat)
{
    // TODO: Consider supporting other decoded video frame formats.
    const auto expectedFormat = AV_PIX_FMT_YUV420P;
    if (frame->format != expectedFormat)
    {
        NX_ERROR(typeid(FrameConverter),
            "Ignoring decoded frame: AVPixelFormat is %1 instead of %2.",
            frame->format, expectedFormat);
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

IDataPacket* FrameConverter::getDataPacket(std::optional<PixelFormat> pixelFormat)
{
    if (!pixelFormat)
    {
        if (const auto compressedFrame = m_getCompressedFrame())
        {
            if (!m_compressedFrame)
                m_compressedFrame.reset(new WrappingCompressedVideoPacket(compressedFrame));

            return m_compressedFrame.get();
        }
        return nullptr;
    }

    if (const auto uncompressedFrame = m_getUncompressedFrame())
    {
        auto it = m_uncompressedFrames.find(*pixelFormat);
        if (it == m_uncompressedFrames.cend())
        {
            auto insertResult = m_uncompressedFrames.emplace(
                *pixelFormat,
                nx::sdk::common::Ptr<IUncompressedVideoFrame>(
                    createUncompressedVideoFrameFromVideoDecoderOutput(
                        uncompressedFrame, *pixelFormat)));
            it = insertResult.first;
        }
        return it->second.get();
    }
    return nullptr;
}

} // namespace analytics
} // namespace vms::server
} // namespace nx
