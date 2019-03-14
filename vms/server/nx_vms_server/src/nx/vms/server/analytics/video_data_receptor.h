#pragma once

#include <set>

#include <nx/streaming/video_data_packet.h>
#include <utils/media/frame_info.h>
#include "nx/sdk/analytics/i_uncompressed_video_frame.h"

namespace nx {
namespace vms::server {
namespace analytics {

class VideoDataReceptor
{
public:
    using PixelFormat = nx::sdk::analytics::IUncompressedVideoFrame::PixelFormat;

    /**
     * Accept a video frame.
     * @param compressedFrame Not null.
     * @param uncompressedFrame Can be null if uncompressed frame will not be needed.
     */
    using Callback = std::function<void(
        const QnConstCompressedVideoDataPtr& compressedFrame,
        const CLConstVideoDecoderOutputPtr& uncompressedFrame)>;

    using NeededUncompressedPixelFormats = std::set<PixelFormat>;

    VideoDataReceptor(
        Callback callback,
        bool needsCompressedFrames,
        NeededUncompressedPixelFormats neededUncompressedPixelFormats)
        :
        m_callback(callback),
        m_needsCompressedFrames(needsCompressedFrames),
        m_neededUncompressedPixelFormats(neededUncompressedPixelFormats)
    {
    }

    bool needsCompressedFrames() const { return m_needsCompressedFrames; }

    NeededUncompressedPixelFormats neededUncompressedPixelFormats() const
    {
        return m_neededUncompressedPixelFormats;
    }

    /**
     * Call the callback for a video frame.
     * @param compressedFrame Not null.
     * @param uncompressedFrame Can be null if uncompressed frame will not be needed.
     */
    void putFrame(
        const QnConstCompressedVideoDataPtr& compressedFrame,
        const CLConstVideoDecoderOutputPtr& uncompressedFrame)
    {
        m_callback(compressedFrame, uncompressedFrame);
    }

private:
    Callback m_callback;
    const bool m_needsCompressedFrames;
    const NeededUncompressedPixelFormats m_neededUncompressedPixelFormats;
};
using VideoDataReceptorPtr = QSharedPointer<VideoDataReceptor>;

} // namespace analytics
} // namespace vms::server
} // namespace nx
