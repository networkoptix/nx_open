#pragma once

#include <optional>

#include <nx/utils/move_only_func.h>
#include <nx/streaming/video_data_packet.h>
#include <utils/media/frame_info.h>
#include <nx/sdk/analytics/i_data_packet.h>
#include <nx/sdk/analytics/i_uncompressed_video_frame.h>
#include <nx/vms/server/analytics/compressed_video_packet.h>
#include <nx/sdk/helpers/ptr.h>

namespace nx {
namespace vms::server {
namespace analytics {

/**
 * Converts the frame from compressed and yuv-uncompressed internal format to a plugin-consumable
 * form (an object implementing IDataPacket), converting yuv to rgb when needed. The resulting
 * frames are cached inside the instance of this class.
 */
class FrameConverter
{
public:
    /**
     * @param uncompressedFrame Can be null if it is known that a compressed frame will not be
     *     requested from this instance of FrameConverter.
     * @param missingUncompressedFrameWarningIssued Used to issue the warning only once - when an
     *     uncompressed frame is requested for the first time for the particular video stream, but
     *     was not supplied to the constructor of this class.
     */
    FrameConverter(
        QnConstCompressedVideoDataPtr compressedFrame,
        CLConstVideoDecoderOutputPtr uncompressedFrame,
        bool* missingUncompressedFrameWarningIssued)
        :
        m_compressedFrame(new CompressedVideoPacket(compressedFrame)),
        m_uncompressedFrame(std::move(uncompressedFrame)),
        m_missingUncompressedFrameWarningIssued(missingUncompressedFrameWarningIssued)
    {
    }

    /**
     * @param pixelFormat If omitted, a compressed frame request is assumed.
     * @return Null if the frame is not available.
     */
    nx::sdk::analytics::IDataPacket* getDataPacket(
        std::optional<nx::sdk::analytics::IUncompressedVideoFrame::PixelFormat> pixelFormat);

private:
    nx::sdk::Ptr<CompressedVideoPacket> m_compressedFrame;
    CLConstVideoDecoderOutputPtr m_uncompressedFrame;
    bool* m_missingUncompressedFrameWarningIssued;

    std::map<nx::sdk::analytics::IUncompressedVideoFrame::PixelFormat,
        nx::sdk::Ptr<nx::sdk::analytics::IUncompressedVideoFrame>> m_cachedUncompressedFrames;
};

} // namespace analytics
} // namespace vms::server
} // namespace nx
