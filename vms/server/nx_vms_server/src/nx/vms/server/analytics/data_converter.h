#pragma once

#include <map>

#include <utils/media/frame_info.h>

#include <nx/streaming/abstract_data_packet.h>
#include <nx/vms/server/analytics/stream_requirements.h>

namespace nx::vms::server::analytics {

class DataConverter
{
public:
    DataConverter(int rotationAngle);

    QnAbstractDataPacketPtr convert(
        const QnAbstractDataPacketPtr& dataPacket,
        const StreamRequirements& requirements);

private:
    QnAbstractDataPacketPtr getUncompressedFrame(
        CLConstVideoDecoderOutputPtr decoderOutput,
        const StreamRequirements& requirements);

private:
    const int m_rotationAngle;
    std::map<AVPixelFormat, QnAbstractDataPacketPtr> m_cachedUncompressedFrames;
};

} // namespace nx::vms::server::analytics
