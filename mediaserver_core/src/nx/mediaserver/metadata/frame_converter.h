#pragma once

#include <boost/optional.hpp>

#include <nx/utils/move_only_func.h>
#include <nx/streaming/video_data_packet.h>
#include <utils/media/frame_info.h>
#include <nx/sdk/metadata/data_packet.h>
#include <nx/sdk/metadata/uncompressed_video_frame.h>
#include <nx/mediaserver/metadata/wrapping_compressed_video_packet.h>
#include <plugins/plugin_tools.h>

namespace nx {
namespace mediaserver {
namespace metadata {

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
    nx::sdk::metadata::DataPacket* getDataPacket(
        boost::optional<nx::sdk::metadata::UncompressedVideoFrame::PixelFormat> pixelFormat);

private:
    nx::utils::MoveOnlyFunc<QnConstCompressedVideoDataPtr()> m_getCompressedFrame;
    nx::utils::MoveOnlyFunc<CLConstVideoDecoderOutputPtr()> m_getUncompressedFrame;
    
    std::map<nx::sdk::metadata::UncompressedVideoFrame::PixelFormat,
        nxpt::ScopedRef<nx::sdk::metadata::UncompressedVideoFrame>> m_uncompressedFrames;

    nxpt::ScopedRef<nx::sdk::metadata::WrappingCompressedVideoPacket> m_compressedFrame;
};

} // namespace metadata
} // namespace mediaserver
} // namespace nx
