#pragma once

#include <set>

#include <nx/streaming/video_data_packet.h>
#include <utils/media/frame_info.h>
#include "nx/sdk/metadata/uncompressed_video_frame.h"

namespace nx {
namespace mediaserver {
namespace metadata {

class VideoDataReceptor
{
public:
    using PixelFormat = nx::sdk::metadata::UncompressedVideoFrame::PixelFormat;

    /**
     * Accept a video frame. Either one of the supplied pointers may be null if the respective
     * frame kind was not requested by acceptedFrameKind().
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
     * Call the callback for a video frame. Either one of the supplied pointers may be null if the
     * respective frame kind is not in acceptedFrameKinds().
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

} // namespace metadata
} // namespace mediaserver
} // namespace nx
