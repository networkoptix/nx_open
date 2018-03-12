#pragma once

#include <nx/streaming/video_data_packet.h>
#include <nx/sdk/metadata/common_compressed_video_packet.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <utils/media/frame_info.h>
#include "nx/sdk/metadata/uncompressed_video_frame.h"

namespace nx {
namespace mediaserver {
namespace metadata {

class VideoDataReceptor
{
public:
    /**
     * Accept a video frame. Either one of the supplied pointers may be null if the respective
     * frame kind was not requested by acceptedFrameKind().
     */
    using Callback = std::function<void(
        const QnConstCompressedVideoDataPtr& compressedFrame,
        const CLConstVideoDecoderOutputPtr& uncompressedFrame)>;

    /**
     * Should be called from the callback to copy the incoming data to the SDK format. To obtain
     * multiple copies of the same data, should be called multiple times.
     * @return Null if the data is null.
     */
    static nx::sdk::metadata::CommonCompressedVideoPacket* compressedVideoDataToPlugin(
        const QnConstCompressedVideoDataPtr& frame, bool needDeepCopy);

    /**
     * Should be called from the callback to copy the incoming data to the SDK format. To obtain
     * multiple copies of the same data, should be called multiple times.
     * @return Null if the data is null.
     */
    static nx::sdk::metadata::UncompressedVideoFrame* videoDecoderOutputToPlugin(
        const CLConstVideoDecoderOutputPtr& frame, bool needDeepCopy);

    enum class AcceptedFrameKind { compressed, uncompressed };

    VideoDataReceptor(Callback callback, AcceptedFrameKind acceptedFrameKind):
        m_callback(callback), m_acceptedFrameKind(acceptedFrameKind)
    {
    }

    AcceptedFrameKind acceptedFrameKind() const { return m_acceptedFrameKind; }

    /**
     * Call the callback for a video frame. Either one of the supplied pointers may be null if the
     * respective frame kind was not requested by acceptedFrameKind().
     */
    void putFrame(
        const QnConstCompressedVideoDataPtr& compressedFrame,
        const CLConstVideoDecoderOutputPtr& uncompressedFrame)
    {
        m_callback(compressedFrame, uncompressedFrame);
    }

private:
    static nx::sdk::metadata::UncompressedVideoFrame* convertToYuv420pSdkFrame(
        CLConstVideoDecoderOutputPtr frame,
        bool needDeepCopy);

    static nx::sdk::metadata::UncompressedVideoFrame* convertToBgraSdkFrame(
        CLConstVideoDecoderOutputPtr frame,
        bool needDeepCopy);

private:
    Callback m_callback;
    AcceptedFrameKind m_acceptedFrameKind = AcceptedFrameKind::compressed;
};
using VideoDataReceptorPtr = QSharedPointer<VideoDataReceptor>;

} // namespace metadata
} // namespace mediaserver
} // namespace nx
