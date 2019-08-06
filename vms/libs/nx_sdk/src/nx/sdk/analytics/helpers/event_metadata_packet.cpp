// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_metadata_packet.h"

#include <nx/kit/debug.h>

namespace nx {
namespace sdk {
namespace analytics {

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

const IEventMetadata* EventMetadataPacket::getAt(int index) const
{
    if (index < 0 || index >= (int) m_events.size())
        return nullptr;

    auto& eventMetadata = m_events[index];
    eventMetadata->addRef();
    return eventMetadata.get();
}

void EventMetadataPacket::setTimestampUs(int64_t timestampUs)
{
    m_timestampUs = timestampUs;
}

void EventMetadataPacket::setDurationUs(int64_t durationUs)
{
    m_durationUs = durationUs;
}

void EventMetadataPacket::addItem(const IEventMetadata* eventMetadata)
{
    NX_KIT_ASSERT(eventMetadata);
    eventMetadata->addRef();
    m_events.push_back(toPtr(eventMetadata));
}

void EventMetadataPacket::clear()
{
    m_events.clear();
}

} // namespace analytics
} // namespace sdk
} // namespace nx
