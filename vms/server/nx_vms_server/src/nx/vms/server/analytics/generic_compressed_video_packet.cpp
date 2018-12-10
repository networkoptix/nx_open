#include "generic_compressed_video_packet.h"

namespace nx {
namespace vms::server {
namespace analytics {

using namespace  nx::sdk::analytics;

void* GenericCompressedVideoPacket::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_CompressedVideoPacket)
    {
        addRef();
        return static_cast<CompressedVideoPacket*>(this);
    }
    if (interfaceId == IID_CompressedMediaPacket)
    {
        addRef();
        return static_cast<CompressedMediaPacket*>(this);
    }
    if (interfaceId == IID_CompressedMediaPacket)
    {
        addRef();
        return static_cast<CompressedMediaPacket*>(this);
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

} // namespace analytics
} // namespace vms::server
} // namespace nx
