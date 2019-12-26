#pragma once

#include <map>

#include <utils/media/frame_info.h>

#include <nx/sdk/ptr.h>
#include <nx/sdk/analytics/i_data_packet.h>
#include <nx/sdk/analytics/i_uncompressed_video_frame.h>

#include <nx/streaming/abstract_data_packet.h>
#include <nx/vms/server/analytics/stream_requirements.h>

namespace nx::vms::server::analytics {

class DataConverter
{
public:
    explicit DataConverter(int rotationAngle);

    QnAbstractDataPacketPtr convert(
        const QnAbstractDataPacketPtr& dataPacket,
        const QnConstMetaDataV1Ptr& associatedMotionMetadata,
        const StreamRequirements& requirements);

private:
    QnAbstractDataPacketPtr getUncompressedFrame(
        CLConstVideoDecoderOutputPtr decoderOutput,
        const QnConstMetaDataV1Ptr& associatedMotionMetadata,
        const StreamRequirements& requirements);

private:
    int m_rotationAngle;
    std::map<AVPixelFormat, QnAbstractDataPacketPtr> m_cachedUncompressedFrames;
};

} // namespace nx::vms::server::analytics
