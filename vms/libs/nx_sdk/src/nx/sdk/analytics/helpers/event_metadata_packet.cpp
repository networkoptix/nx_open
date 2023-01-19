// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_metadata_packet.h"

#include <nx/kit/debug.h>

namespace nx::sdk::analytics {

EventMetadataPacket::Flags EventMetadataPacket::flags() const
{
    return m_flags;
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

const IEventMetadata* EventMetadataPacket::getAt(int index) const
{
    if (index < 0 || index >= (int) m_events.size())
        return nullptr;

    return shareToPtr(m_events[index]).releasePtr();
}

void EventMetadataPacket::setFlags(Flags flags)
{
    m_flags = flags;
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
    if (!NX_KIT_ASSERT(eventMetadata))
        return;
    m_events.push_back(shareToPtr(eventMetadata));
}

void EventMetadataPacket::clear()
{
    m_events.clear();
}

} // namespace nx::sdk::analytics
