#include "object_metadata_packet.h"

#include <nx/kit/debug.h>

namespace nx {
namespace sdk {
namespace analytics {

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

const IObjectMetadata* ObjectMetadataPacket::at(int index) const
{
    if (index < 0 || index >= (int) m_objects.size())
        return nullptr;

    return m_objects[index];
}

void ObjectMetadataPacket::setTimestampUs(int64_t timestampUs)
{
    m_timestampUs = timestampUs;
}

void ObjectMetadataPacket::setDurationUs(int64_t durationUs)
{
    m_durationUs = durationUs;
}

void ObjectMetadataPacket::addItem(const IObjectMetadata* object)
{
    NX_KIT_ASSERT(object);
    m_objects.push_back(object);
}

void ObjectMetadataPacket::clear()
{
    m_objects.clear();
}

} // namespace analytics
} // namespace sdk
} // namespace nx
