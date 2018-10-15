#pragma once

#include <set>

#include <nx/streaming/video_data_packet.h>
#include <utils/media/frame_info.h>
#include <nx/sdk/analytics/uncompressed_video_frame.h>

namespace nx::mediaserver::analytics {

// TODO: This class has really awful interface. Refactor it.

class AbstractVideoDataReceptor
{
public:
    using PixelFormat = nx::sdk::analytics::UncompressedVideoFrame::PixelFormat;
    using NeededUncompressedPixelFormats = std::set<PixelFormat>;

    virtual bool needsCompressedFrames() const = 0;
    virtual NeededUncompressedPixelFormats neededUncompressedPixelFormats() const = 0;

    /**
    * Call the callback for a video frame.
    * @param compressedFrame Not null.
    * @param uncompressedFrame Can be null if uncompressed frame will not be needed.
    */
    virtual void putFrame(
        const QnConstCompressedVideoDataPtr& compressedFrame,
        const CLConstVideoDecoderOutputPtr& uncompressedFrame) = 0;

};

using AbstractVideoDataReceptorPtr = QSharedPointer<AbstractVideoDataReceptor>;

} // namespace nx::mediaserver::analytics
