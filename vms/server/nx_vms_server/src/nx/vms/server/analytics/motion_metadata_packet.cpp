#include "motion_metadata_packet.h"

#include <nx/utils/log/log.h>

namespace nx::vms::server::analytics {

MotionMetadataPacket::MotionMetadataPacket(QnConstMetaDataV1Ptr motionMetadata):
    m_motionMetadata(motionMetadata)
{
}

const uint8_t* MotionMetadataPacket::motionData() const
{
    if (NX_ASSERT(m_motionMetadata))
        return (uint8_t*) m_motionMetadata->data();

    return nullptr;
}

int MotionMetadataPacket::motionDataSize() const
{
    if (NX_ASSERT(m_motionMetadata))
        return (int) m_motionMetadata->dataSize();

    return 0;
}

int MotionMetadataPacket::rowCount() const
{
    if (NX_ASSERT(m_motionMetadata))
        return Qn::kMotionGridHeight;

    return 0;
}

int MotionMetadataPacket::columnCount() const
{
    if (NX_ASSERT(m_motionMetadata))
        return Qn::kMotionGridWidth;

    return 0;
}

bool MotionMetadataPacket::isEmpty() const
{
    if (NX_ASSERT(m_motionMetadata))
        return m_motionMetadata->isEmpty();

    return false;
}

bool MotionMetadataPacket::isMotionAt(int x, int y) const
{
    if (NX_ASSERT(m_motionMetadata))
        return m_motionMetadata->isMotionAt(x, y);

    return nullptr;
}

int64_t MotionMetadataPacket::timestampUs() const
{
    if (NX_ASSERT(m_motionMetadata))
        return m_motionMetadata->timestamp;

    return 0;
}

} // namespace nx::vms::server::analytics
