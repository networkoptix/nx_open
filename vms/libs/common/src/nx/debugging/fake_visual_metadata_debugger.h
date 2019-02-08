#pragma once

#include <nx/debugging/abstract_visual_metadata_debugger.h>

namespace nx {
namespace debugging {

class FakeVisualMetadataDebugger: public AbstractVisualMetadataDebugger
{
public:
    virtual void push(const QnConstCompressedVideoDataPtr& video) override;
    virtual void push(
        const nx::common::metadata::DetectionMetadataPacketPtr& detectionMetadata) override;
    virtual void push(const CLConstVideoDecoderOutputPtr& frame) override;
    virtual void push(const QnConstCompressedMetadataPtr& metadata) override;
};

} // namespace debugging
} // namespace nx
