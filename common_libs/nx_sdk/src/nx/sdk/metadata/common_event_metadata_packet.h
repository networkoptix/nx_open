#pragma once

#include <vector>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/events_metadata_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

class /*NX_SDK_API*/ CommonEventMetadataPacket: public nxpt::CommonRefCounter<EventsMetadataPacket>
{
public:
    virtual ~CommonEventMetadataPacket();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual int64_t timestampUsec() const override;

    virtual int64_t durationUsec() const override;

    virtual Event* nextItem() override;

    void setTimestampUsec(int64_t timestampUsec);

    void setDurationUsec(int64_t durationUsec);

    void addEvent(Event* event);

    void resetEvents();

private:
    int64_t m_timestampUsec = -1;
    int64_t m_durationUsec = -1;

    std::vector<Event*> m_events;
    int m_currentEventIndex = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx