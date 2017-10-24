#include "common_event_metadata_packet.h"

namespace nx {
namespace sdk {
namespace metadata {

void* CommonMetadataPacket::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_EventMetadataPacket)
    {
        addRef();
        return static_cast<AbstractIterableMetadataPacket*>(this);
    }

    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

int64_t CommonMetadataPacket::timestampUsec() const
{
    return m_timestampUsec;
}

int64_t CommonMetadataPacket::durationUsec() const
{
    return m_durationUsec;
}

AbstractMetadataItem* CommonMetadataPacket::nextItem()
{
    if (m_currentEventIndex < m_events.size())
        return m_events[m_currentEventIndex++];

    return nullptr;
}

void CommonMetadataPacket::setTimestampUsec(int64_t timestampUsec)
{
    m_timestampUsec = timestampUsec;
}

void CommonMetadataPacket::setDurationUsec(int64_t durationUsec)
{
    m_durationUsec = durationUsec;
}

void CommonMetadataPacket::addEvent(AbstractMetadataItem* event)
{
    m_events.push_back(event);
}

void CommonMetadataPacket::resetEvents()
{
    m_events.clear();
    m_currentEventIndex = 0;
}

CommonMetadataPacket::~CommonMetadataPacket()
{
}

} // namespace metadata
} // namespace sdk
} // namespace nx
