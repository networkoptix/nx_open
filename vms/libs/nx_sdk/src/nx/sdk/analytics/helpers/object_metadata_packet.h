#pragma once

#include <vector>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/analytics/i_object_metadata_packet.h>

namespace nx {
namespace sdk {
namespace analytics {

class ObjectMetadataPacket: public RefCountable<IObjectMetadataPacket>
{
public:
    virtual int64_t timestampUs() const override;
    virtual int64_t durationUs() const override;
    virtual int count() const override;
    virtual const IObjectMetadata* at(int index) const override;

    void setTimestampUs(int64_t timestampUs);
    void setDurationUs(int64_t durationUs);
    void addItem(const IObjectMetadata* object);
    void clear();

private:
    int64_t m_timestampUs = -1;
    int64_t m_durationUs = -1;

    std::vector<Ptr<const IObjectMetadata>> m_objects;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
