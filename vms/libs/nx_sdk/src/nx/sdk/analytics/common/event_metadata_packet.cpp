#include "event_metadata_packet.h"

namespace nx {
namespace sdk {
namespace analytics {
namespace common {

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

IEvent* EventMetadataPacket::nextItem()
{
    if (m_currentIndex < m_events.size())
        return m_events[m_currentIndex++];

    return nullptr;
}

void EventMetadataPacket::setTimestampUs(int64_t timestampUs)
{
    m_timestampUs = timestampUs;
}

void EventMetadataPacket::setDurationUs(int64_t durationUs)
{
    m_durationUs = durationUs;
}

void EventMetadataPacket::addItem(IEvent* event)
{
    m_events.push_back(event);
}

void EventMetadataPacket::clear()
{
    m_events.clear();
    m_currentIndex = 0;
}

EventMetadataPacket::~EventMetadataPacket()
{
}

} // namespace common
} // namespace analytics
} // namespace sdk
} // namespace nx
