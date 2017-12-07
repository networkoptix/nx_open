#pragma once

#include <vector>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/abstract_metadata_packet.h>
#include <nx/sdk/metadata/abstract_event_metadata_packet.h>
#include "abstract_detection_metadata_packet.h"

namespace nx {
namespace sdk {
namespace metadata {

template <class T, class ItemType>
class CommonMetadataPacketBase: public nxpt::CommonRefCounter<T>
{
public:
    virtual ~CommonMetadataPacketBase() {}

    virtual int64_t timestampUsec() const override { return m_timestampUsec; }

    virtual int64_t durationUsec() const override { return m_durationUsec; }

    virtual ItemType* nextItem() override
    {
        return m_index < m_items.size() ? m_items[m_index++] : nullptr;
    }

    void setTimestampUsec(int64_t timestampUsec) { m_timestampUsec = timestampUsec; }

    void setDurationUsec(int64_t durationUsec) { m_durationUsec = durationUsec; }

    void addItem(ItemType* item) { m_items.push_back(item); }

    void resetItems() { m_index = 0; m_items.clear(); }

private:
    int64_t m_timestampUsec = -1;
    int64_t m_durationUsec = -1;

    std::vector<ItemType*> m_items;
    int m_index = 0;
};

class CommonEventsMetadataPacket:
    public CommonMetadataPacketBase<AbstractEventMetadataPacket, AbstractDetectedEvent>
{
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

};

class CommonObjectsMetadataPacket:
    public CommonMetadataPacketBase<AbstractObjectsMetadataPacket, AbstractDetectedObject>
{
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
