#pragma once

#include <vector>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/abstract_metadata_packet.h>
#include <nx/sdk/metadata/abstract_event_metadata_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

class CommonMetadataPacket: public nxpt::CommonRefCounter<AbstractIterableMetadataPacket>
{
public:
    virtual ~CommonMetadataPacket();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual int64_t timestampUsec() const override;

    virtual int64_t durationUsec() const override;

    virtual AbstractMetadataItem* nextItem() override;

    void setTimestampUsec(int64_t timestampUsec);

    void setDurationUsec(int64_t durationUsec);

    void addEvent(AbstractMetadataItem* event);

    void resetEvents();

private:
    int64_t m_timestampUsec = -1;
    int64_t m_durationUsec = -1;

    std::vector<AbstractMetadataItem*> m_events;
    int m_currentEventIndex = 0;

};

} // namespace metadata
} // namespace sdk
} // namespace nx