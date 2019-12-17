#pragma once

#include <nx/streaming/media_data_packet.h>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/analytics/i_motion_metadata_packet.h>

namespace nx::vms::server::analytics {

class MotionMetadataPacket:
    public nx::sdk::RefCountable<nx::sdk::analytics::IMotionMetadataPacket>
{
public:
    MotionMetadataPacket(QnConstMetaDataV1Ptr motionMetadata);

    virtual const uint8_t* motionData() const override;

    virtual int motionDataSize() const override;

    virtual int rowCount() const override;

    virtual int columnCount() const override;

    virtual bool isEmpty() const override;

    virtual bool isMotionAt(int columnIndex, int rowIndex) const override;

    virtual int64_t timestampUs() const override;

private:
    QnConstMetaDataV1Ptr m_motionMetadata;
};

} // namespace nx::vms::server::analytics
