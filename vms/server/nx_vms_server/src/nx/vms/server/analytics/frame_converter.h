#pragma once

#include <boost/optional.hpp>

#include <nx/utils/move_only_func.h>
#include <nx/streaming/video_data_packet.h>
#include <utils/media/frame_info.h>
#include <nx/sdk/analytics/data_packet.h>
#include <nx/sdk/analytics/uncompressed_video_frame.h>
#include <nx/vms/server/analytics/wrapping_compressed_video_packet.h>
#include <plugins/plugin_tools.h>

namespace nx {
namespace vms::server {
namespace analytics {

/**
 * Converts the frame from compressed and yuv-uncompressed internal format to a plugin-consumable
 * form (an object implementing DataPacket), converting yuv to rgb when needed. The resulting
 * frames are cached inside the instance of this class.
 */
class FrameConverter
{
public:
    FrameConverter(
        nx::utils::MoveOnlyFunc<QnConstCompressedVideoDataPtr()> getCompressedFrame,
        nx::utils::MoveOnlyFunc<CLConstVideoDecoderOutputPtr()> getUncompressedFrame)
        :
        m_getCompressedFrame(std::move(getCompressedFrame)),
        m_getUncompressedFrame(std::move(getUncompressedFrame))
    {
    }

    /**
     * @param pixelFormat If omitted, compressed frame request is assumed.
     * @return Null if the frame is not available.
     */
    nx::sdk::analytics::DataPacket* getDataPacket(
        std::optional<nx::sdk::analytics::UncompressedVideoFrame::PixelFormat> pixelFormat);

private:
    nx::utils::MoveOnlyFunc<QnConstCompressedVideoDataPtr()> m_getCompressedFrame;
    nx::utils::MoveOnlyFunc<CLConstVideoDecoderOutputPtr()> m_getUncompressedFrame;

    std::map<nx::sdk::analytics::UncompressedVideoFrame::PixelFormat,
        nxpt::ScopedRef<nx::sdk::analytics::UncompressedVideoFrame>> m_uncompressedFrames;

    nxpt::ScopedRef<WrappingCompressedVideoPacket> m_compressedFrame;
};

} // namespace analytics
} // namespace vms::server
} // namespace nx
