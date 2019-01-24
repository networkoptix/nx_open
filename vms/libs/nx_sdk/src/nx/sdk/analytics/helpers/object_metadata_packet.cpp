#include "object_metadata_packet.h"

namespace nx {
namespace sdk {
namespace analytics {

void* ObjectMetadataPacket::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_ObjectMetadataPacket)
    {
        addRef();
        return static_cast<IObjectMetadataPacket*>(this);
    }

    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
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
    return m_objects.size();
}

const IObjectMetadata* ObjectMetadataPacket::at(int index) const
{
    if (index < 0 || index >= m_objects.size())
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

void ObjectMetadataPacket::addItem(IObjectMetadata* object)
{
    m_objects.push_back(object);
}

void ObjectMetadataPacket::clear()
{
    m_objects.clear();
}

} // namespace analytics
} // namespace sdk
} // namespace nx
