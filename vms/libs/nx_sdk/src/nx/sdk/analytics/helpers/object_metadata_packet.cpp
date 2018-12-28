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

IObject* ObjectMetadataPacket::nextItem()
{
    if (m_currentIndex < (int) m_objects.size())
        return m_objects[m_currentIndex++];

    return nullptr;
}

void ObjectMetadataPacket::setTimestampUs(int64_t timestampUs)
{
    m_timestampUs = timestampUs;
}

void ObjectMetadataPacket::setDurationUs(int64_t durationUs)
{
    m_durationUs = durationUs;
}

void ObjectMetadataPacket::addItem(IObject* object)
{
    m_objects.push_back(object);
}

void ObjectMetadataPacket::clear()
{
    m_objects.clear();
    m_currentIndex = 0;
}

ObjectMetadataPacket::~ObjectMetadataPacket()
{
}

} // namespace analytics
} // namespace sdk
} // namespace nx
