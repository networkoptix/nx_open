#include "common_event_metadata_packet.h"

namespace nx {
namespace sdk {
namespace metadata {

void* CommonEventMetadataPacket::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_EventMetadataPacket)
    {
        addRef();
        return static_cast<AbstractEventMetadataPacket*>(this);
    }

    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

int64_t CommonEventMetadataPacket::timestampUsec() const
{
    return m_timestampUsec;
}

int64_t CommonEventMetadataPacket::durationUsec() const
{
    return m_durationUsec;
}

AbstractDetectedEvent* CommonEventMetadataPacket::nextItem()
{
    if (m_currentEventIndex < m_events.size())
        return m_events[m_currentEventIndex++];

    return nullptr;
}

void CommonEventMetadataPacket::setTimestampUsec(int64_t timestampUsec)
{
    m_timestampUsec = timestampUsec;
}

void CommonEventMetadataPacket::setDurationUsec(int64_t durationUsec)
{
    m_durationUsec = durationUsec;
}

void CommonEventMetadataPacket::addEvent(AbstractDetectedEvent* event)
{
    m_events.push_back(event);
}

void CommonEventMetadataPacket::resetEvents()
{
    m_events.clear();
    m_currentEventIndex = 0;
}

CommonEventMetadataPacket::~CommonEventMetadataPacket()
{
    qDebug() << "####### Destroying packet!!!!!";
}

} // namespace metadata
} // namespace sdk
} // namespace nx
