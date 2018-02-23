#include "common_metadata_packet.h"

namespace nx {
namespace sdk {
namespace metadata {

void* CommonEventsMetadataPacket::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_EventsMetadataPacket)
    {
        addRef();
        return static_cast<EventsMetadataPacket*>(this);
    }
    if (interfaceId == IID_IterableMetadataPacket)
    {
        addRef();
        return static_cast<IterableMetadataPacket*>(this);
    }
    if (interfaceId == IID_MetadataPacket)
    {
        addRef();
        return static_cast<MetadataPacket*>(this);
    }
    if (interfaceId == IID_DataPacket)
    {
        addRef();
        return static_cast<DataPacket*>(this);
    }
    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

void* CommonObjectsMetadataPacket::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_ObjectsMetadataPacket)
    {
        addRef();
        return static_cast<ObjectsMetadataPacket*>(this);
    }
    if (interfaceId == IID_IterableMetadataPacket)
    {
        addRef();
        return static_cast<ObjectsMetadataPacket*>(this);
    }
    if (interfaceId == IID_MetadataPacket)
    {
        addRef();
        return static_cast<MetadataPacket*>(this);
    }
    if (interfaceId == IID_DataPacket)
    {
        addRef();
        return static_cast<DataPacket*>(this);
    }
    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

} // namespace metadata
} // namespace sdk
} // namespace nx
