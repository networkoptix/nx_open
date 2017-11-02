#include "common_metadata_packet.h"

namespace nx {
namespace sdk {
namespace metadata {

void* CommonEventsMetadataPacket::queryInterface(const nxpl::NX_GUID& interfaceId)
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

void* CommonObjectsMetadataPacket::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_DetectionMetadataPacket)
    {
        addRef();
        return static_cast<AbstractObjectsMetadataPacket*>(this);
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
