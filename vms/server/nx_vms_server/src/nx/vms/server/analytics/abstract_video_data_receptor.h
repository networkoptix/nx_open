#pragma once

#include <set>

#include <nx/streaming/video_data_packet.h>
#include <utils/media/frame_info.h>
#include <nx/sdk/analytics/i_uncompressed_video_frame.h>

namespace nx::vms::server::analytics {

// TODO: This class has really awful interface. Refactor it.

class AbstractVideoDataReceptor
{
public:
    using PixelFormat = nx::sdk::analytics::IUncompressedVideoFrame::PixelFormat;
    using NeededUncompressedPixelFormats = std::set<PixelFormat>;

    virtual ~AbstractVideoDataReceptor() = default;

    virtual bool needsCompressedFrames() const = 0;
    virtual NeededUncompressedPixelFormats neededUncompressedPixelFormats() const = 0;

    /**
     * Call the callback for a video frame.
     * @param compressedFrame Not null.
     * @param uncompressedFrame Can be null if uncompressed frame would not be needed.
     */
    virtual void putFrame(
        const QnConstCompressedVideoDataPtr& compressedFrame,
        const CLConstVideoDecoderOutputPtr& uncompressedFrame) = 0;
};

using AbstractVideoDataReceptorPtr = QSharedPointer<AbstractVideoDataReceptor>;

} // namespace nx::vms::server::analytics
