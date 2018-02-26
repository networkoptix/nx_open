#pragma once

#include <vector>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/objects_metadata_packet.h>
#include <nx/sdk/metadata/events_metadata_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

template <class SomeIterableMetadataPacket, class Item>
class CommonMetadataPacketBase: public nxpt::CommonRefCounter<SomeIterableMetadataPacket>
{
public:
    virtual ~CommonMetadataPacketBase() {}

    virtual int64_t timestampUsec() const override { return m_timestampUsec; }

    virtual int64_t durationUsec() const override { return m_durationUsec; }

    virtual Item* nextItem() override
    {
        return (m_index < (int) m_items.size()) ? m_items[m_index++] : nullptr;
    }

    void setTimestampUsec(int64_t timestampUsec) { m_timestampUsec = timestampUsec; }

    void setDurationUsec(int64_t durationUsec) { m_durationUsec = durationUsec; }

    void addItem(Item* item) { m_items.push_back(item); }

    void resetItems() { m_index = 0; m_items.clear(); }

private:
    int64_t m_timestampUsec = -1;
    int64_t m_durationUsec = -1;

    std::vector<Item*> m_items;
    int m_index = 0;
};

class CommonEventsMetadataPacket:
    public CommonMetadataPacketBase<EventsMetadataPacket, Event>
{
public:
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;
};

class CommonObjectsMetadataPacket:
    public CommonMetadataPacketBase<ObjectsMetadataPacket, Object>
{
public:
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;
};

} // namespace metadata
} // namespace sdk
} // namespace nx