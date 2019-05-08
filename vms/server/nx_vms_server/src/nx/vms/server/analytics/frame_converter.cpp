#include "frame_converter.h"

#include <utils/media/ffmpeg_helper.h>
#include <nx/utils/log/log.h>
#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/analytics/helpers/pixel_format.h>
#include <nx/vms/server/analytics/uncompressed_video_frame.h>
#include <nx/vms/server/sdk_support/utils.h>

namespace nx {
namespace vms::server {
namespace analytics {

using namespace nx::sdk::analytics;
using PixelFormat = IUncompressedVideoFrame::PixelFormat;

static nx::sdk::Ptr<IUncompressedVideoFrame> createUncompressedVideoFrameFromVideoDecoderOutput(
    const CLConstVideoDecoderOutputPtr& frame,
    PixelFormat pixelFormat)
{
    if (!NX_ASSERT(frame))
        return nullptr;

    const auto avPixelFormat = nx::vms::server::sdk_support::sdkToAvPixelFormat(pixelFormat);
    if (avPixelFormat == AV_PIX_FMT_NONE)
        return nullptr; //< Assertion failed.

    if (avPixelFormat == frame->format)
    {
        const auto uncompressedVideoFrame = nx::sdk::makePtr<UncompressedVideoFrame>(frame);

        if (!uncompressedVideoFrame->avFrame())
            return nullptr; //< An assertion already failed.

        return uncompressedVideoFrame;
    }

    const auto uncompressedVideoFrame = nx::sdk::makePtr<UncompressedVideoFrame>(
        frame->width, frame->height, avPixelFormat, frame->pkt_dts);

    if (!uncompressedVideoFrame->avFrame()
        || !frame->convertTo(uncompressedVideoFrame->avFrame()))
    {
        // An assertion already failed or an error message printed.
        return nullptr;
    }

    return uncompressedVideoFrame;
}

nx::sdk::Ptr<IDataPacket> FrameConverter::getDataPacket(std::optional<PixelFormat> pixelFormat)
{
    if (!pixelFormat) //< Compressed frame requested.
        return m_compressedFrame; //< Can be null.

    if (!m_uncompressedFrame) //< Uncompressed frame requested, but is not available.
    {
        // First time log as Warning, other times log as Verbose.
        auto logLevel = nx::utils::log::Level::verbose;
        if (!*m_missingUncompressedFrameWarningIssued)
        {
            *m_missingUncompressedFrameWarningIssued = true;
            logLevel = nx::utils::log::Level::warning;
        }
        NX_UTILS_LOG(logLevel, this, "Uncompressed frame requested but not received.");

        return nullptr;
    }

    auto it = m_cachedUncompressedFrames.find(*pixelFormat);
    if (it == m_cachedUncompressedFrames.cend()) //< Not found in the cache.
    {
        const auto insertResult = m_cachedUncompressedFrames.emplace(
            *pixelFormat,
            // Can be null if the conversion fails; cache it anyway to avoid retrying next time.
            createUncompressedVideoFrameFromVideoDecoderOutput(m_uncompressedFrame, *pixelFormat));

        it = insertResult.first;
    }

    return it->second; //< Can be null.
}

} // namespace analytics
} // namespace vms::server
} // namespace nx
