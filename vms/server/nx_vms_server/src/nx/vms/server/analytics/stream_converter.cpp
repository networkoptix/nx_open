#include "stream_converter.h"

#include <chrono>

#include <nx/utils/log/log.h>

namespace nx::vms::server::analytics {

static const std::chrono::seconds kMotionExpirationTimeout{3};

StreamConverter::StreamConverter():
    m_dataConverter(m_rotationAngleDegrees)
{
}

bool StreamConverter::pushData(QnAbstractDataPacketPtr dataPacket)
{
    if (const auto motionMetadata = std::dynamic_pointer_cast<QnMetaDataV1>(dataPacket))
    {
        NX_VERBOSE(this, "Motion metadata packet has arrived");
        m_lastStreamData.reset();
        m_motionMetadataExpirationTimer.restart();
        m_lastMotionMetadata = motionMetadata;
        return false;
    }

    NX_VERBOSE(this,
        "Got a media data packet (compressed or uncompressed) or in-stream metadata packet");

    m_dataConverter = DataConverter(m_rotationAngleDegrees);
    m_lastStreamData = dataPacket;
    return true;
}

QnAbstractDataPacketPtr StreamConverter::getData(const StreamRequirements& requirements) const
{
    if (!m_lastStreamData)
    {
        NX_VERBOSE(this, "No stream data has arrived yet");
        return nullptr; //< No data to convert.
    }

    QnConstMetaDataV1Ptr motionMetadata = m_lastMotionMetadata;
    if (!m_motionMetadataExpirationTimer.isValid())
    {
        NX_VERBOSE(this, "Motion metadata timer is invalid");
        motionMetadata.reset();
    }
    else if (const std::chrono::milliseconds timeSinceLastMotionMetadata =
        m_motionMetadataExpirationTimer.elapsed();
        timeSinceLastMotionMetadata > kMotionExpirationTimeout)
    {
        NX_VERBOSE(this, "Motion metadata is expired, time since last motion metadata %1 us",
            timeSinceLastMotionMetadata);

        motionMetadata.reset();
    }

    return m_dataConverter.convert(m_lastStreamData, motionMetadata, requirements);
}

void StreamConverter::updateRotation(int rotationAngleDegrees)
{
    NX_VERBOSE(this, "Updating rotation angle to %1 degrees", rotationAngleDegrees);
    m_rotationAngleDegrees = rotationAngleDegrees;
}

} // namespace nx::vms::server::analytics
