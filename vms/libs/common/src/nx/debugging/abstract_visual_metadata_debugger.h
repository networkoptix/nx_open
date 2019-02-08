#pragma once

#include <nx/streaming/video_data_packet.h>

#include <utils/media/frame_info.h>
#include <analytics/common/object_detection_metadata.h>

namespace nx {
namespace debugging {

class AbstractVisualMetadataDebugger
{
public:
    virtual ~AbstractVisualMetadataDebugger() = default;

    virtual void push(const QnConstCompressedVideoDataPtr& video) = 0;
    virtual void push(
        const nx::common::metadata::DetectionMetadataPacketPtr& detectionMetadata) = 0;
    virtual void push(const CLConstVideoDecoderOutputPtr& frame) = 0;
    virtual void push(const QnConstCompressedMetadataPtr& metadata) = 0;
};

using VisualMetadataDebuggerPtr = std::unique_ptr<AbstractVisualMetadataDebugger>;

} // namespace debugging
} // namespace nx
