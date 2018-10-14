#pragma once

#include <set>

#include <utils/media/frame_info.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/sdk/analytics/uncompressed_video_frame.h>
#include <nx/mediaserver/analytics/abstract_video_data_receptor.h>

namespace nx::mediaserver::analytics {

class VideoDataReceptor: public AbstractVideoDataReceptor
{
public:
    using PixelFormat = nx::sdk::analytics::UncompressedVideoFrame::PixelFormat;

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

    virtual bool needsCompressedFrames() const override;

    NeededUncompressedPixelFormats neededUncompressedPixelFormats() const override;

    virtual void putFrame(
        const QnConstCompressedVideoDataPtr& compressedFrame,
        const CLConstVideoDecoderOutputPtr& uncompressedFrame) override;

private:
    const bool m_needsCompressedFrames;
    const NeededUncompressedPixelFormats m_neededUncompressedPixelFormats;
};

} // namespace nx::mediaserver::analytics
