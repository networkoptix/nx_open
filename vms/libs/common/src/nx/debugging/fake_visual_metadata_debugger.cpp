#include "fake_visual_metadata_debugger.h"

namespace nx {
namespace debugging {

void FakeVisualMetadataDebugger::push(const QnConstCompressedVideoDataPtr& /*video*/)
{
    // Do nothing.
}

void FakeVisualMetadataDebugger::push(
    const nx::common::metadata::ObjectMetadataPacketPtr& /*metadataPacket*/)
{
    // Do nothing.
}

void FakeVisualMetadataDebugger::push(const CLConstVideoDecoderOutputPtr& /*frame*/)
{
    // Do nothing.
}

void FakeVisualMetadataDebugger::push(const QnConstCompressedMetadataPtr& /*metadata*/)
{
    // Do nothing.
}

} // namespace debugging
} // namespace nx
