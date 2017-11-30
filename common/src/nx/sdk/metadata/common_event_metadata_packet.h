#pragma once

#include <vector>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/abstract_metadata_packet.h>
#include <nx/sdk/metadata/abstract_event_metadata_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

class CommonEventMetadataPacket: public nxpt::CommonRefCounter<AbstractEventMetadataPacket>
{
public:
    virtual ~CommonEventMetadataPacket();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual int64_t timestampUsec() const override;

    virtual int64_t durationUsec() const override;

    virtual AbstractDetectedEvent* nextItem() override;

    void setTimestampUsec(int64_t timestampUsec);

    void setDurationUsec(int64_t durationUsec);

    void addEvent(AbstractDetectedEvent* event);

    void resetEvents();

private:
    int64_t m_timestampUsec = -1;
    int64_t m_durationUsec = -1;

    std::vector<AbstractDetectedEvent*> m_events;
    int m_currentEventIndex = 0;
    
};

} // namespace metadata
} // namespace sdk
} // namespace nx