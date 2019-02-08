#pragma once

#include <vector>

#include <plugins/plugin_tools.h>
#include <nx/sdk/analytics/i_object_metadata_packet.h>

namespace nx {
namespace sdk {
namespace analytics {
namespace common {

class NX_SDK_API ObjectMetadataPacket: public nxpt::CommonRefCounter<IObjectMetadataPacket>
{
public:
    virtual ~ObjectMetadataPacket();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual int64_t timestampUs() const override;

    virtual int64_t durationUs() const override;

    virtual IObject* nextItem() override;

    void setTimestampUs(int64_t timestampUs);

    void setDurationUs(int64_t durationUs);

    void addItem(IObject* object);

    void clear();

private:
    int64_t m_timestampUs = -1;
    int64_t m_durationUs = -1;

    std::vector<IObject*> m_objects;
    int m_currentIndex = 0;
};

} // namespace common
} // namespace analytics
} // namespace sdk
} // namespace nx
