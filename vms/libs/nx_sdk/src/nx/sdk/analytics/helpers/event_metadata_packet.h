#pragma once

#include <vector>

#include <plugins/plugin_tools.h>
#include <nx/sdk/analytics/i_event_metadata_packet.h>

namespace nx {
namespace sdk {
namespace analytics {

class NX_SDK_API EventMetadataPacket: public nxpt::CommonRefCounter<IEventMetadataPacket>
{
public:
    virtual ~EventMetadataPacket();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual int64_t timestampUs() const override;

    virtual int64_t durationUs() const override;

    virtual IEvent* nextItem() override;

    void setTimestampUs(int64_t timestampUs);

    void setDurationUs(int64_t durationUs);

    void addItem(IEvent* event);

    void clear();

private:
    int64_t m_timestampUs = -1;
    int64_t m_durationUs = -1;

    std::vector<IEvent*> m_events;
    int m_currentIndex = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
