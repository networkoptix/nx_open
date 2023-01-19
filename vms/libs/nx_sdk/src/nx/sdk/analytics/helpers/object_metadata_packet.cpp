// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_metadata_packet.h"

#include <nx/kit/debug.h>

namespace nx::sdk::analytics {

ObjectMetadataPacket::Flags ObjectMetadataPacket::flags() const
{
    return m_flags;
}

int64_t ObjectMetadataPacket::timestampUs() const
{
    return m_timestampUs;
}

int64_t ObjectMetadataPacket::durationUs() const
{
    return m_durationUs;
}

int ObjectMetadataPacket::count() const
{
    return (int) m_objects.size();
}

const IObjectMetadata* ObjectMetadataPacket::getAt(int index) const
{
    if (index < 0 || index >= (int) m_objects.size())
        return nullptr;

    return shareToPtr(m_objects[index]).releasePtr();
}

void ObjectMetadataPacket::setFlags(Flags flags)
{
    m_flags = flags;
}

void ObjectMetadataPacket::setTimestampUs(int64_t timestampUs)
{
    m_timestampUs = timestampUs;
}

void ObjectMetadataPacket::setDurationUs(int64_t durationUs)
{
    m_durationUs = durationUs;
}

void ObjectMetadataPacket::addItem(const IObjectMetadata* objectMetadata)
{
    if (!NX_KIT_ASSERT(objectMetadata))
        return;
    m_objects.push_back(shareToPtr(objectMetadata));
}

void ObjectMetadataPacket::clear()
{
    m_objects.clear();
}

} // namespace nx::sdk::analytics
