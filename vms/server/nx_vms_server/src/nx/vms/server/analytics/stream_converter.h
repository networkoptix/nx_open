#pragma once

#include <nx/utils/elapsed_timer.h>

#include <nx/streaming/media_data_packet.h>

#include  <nx/vms/server/analytics/data_converter.h>

namespace nx::vms::server::analytics {

class StreamConverter
{
public:
    StreamConverter();

    // @return true if it is possible to retrieve some data with `getData()` call after current
    //     `pushData()`
    bool pushData(QnAbstractDataPacketPtr dataPacket);

    QnAbstractDataPacketPtr getData(const StreamRequirements& requirements) const;

    void updateRotation(int rotationAngleDegrees);

private:
    QnAbstractDataPacketPtr m_lastStreamData;
    int m_rotationAngleDegrees = 0;
    nx::utils::ElapsedTimer m_motionMetadataExpirationTimer;
    QnMetaDataV1Ptr m_lastMotionMetadata;

    mutable DataConverter m_dataConverter;
};

} // namespace nx::vms::server::analytics
