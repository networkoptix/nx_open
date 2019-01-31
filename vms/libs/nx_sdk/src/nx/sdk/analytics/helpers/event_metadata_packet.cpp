#include "event_metadata_packet.h"

#include <nx/kit/debug.h>

namespace nx {
namespace sdk {
namespace analytics {

void* EventMetadataPacket::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_EventMetadataPacket)
    {
        addRef();
        return static_cast<IEventMetadataPacket*>(this);
    }

    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

int64_t EventMetadataPacket::timestampUs() const
{
    return m_timestampUs;
}

int64_t EventMetadataPacket::durationUs() const
{
    return m_durationUs;
}

int EventMetadataPacket::count() const
{
    return (int) m_events.size();
}

const IEventMetadata* EventMetadataPacket::at(int index) const
{
    if (index < 0 || index >= (int) m_events.size())
        return nullptr;

    return m_events[index];
}

void EventMetadataPacket::setTimestampUs(int64_t timestampUs)
{
    m_timestampUs = timestampUs;
}

void EventMetadataPacket::setDurationUs(int64_t durationUs)
{
    m_durationUs = durationUs;
}

void EventMetadataPacket::addItem(const IEventMetadata* event)
{
    NX_KIT_ASSERT(event);
    m_events.push_back(event);
}

void EventMetadataPacket::clear()
{
    m_events.clear();
}

} // namespace analytics
} // namespace sdk
} // namespace nx
