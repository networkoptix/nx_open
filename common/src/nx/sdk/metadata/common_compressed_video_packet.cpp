#include "common_compressed_video_packet.h"

namespace nx {
namespace sdk {
namespace metadata {

void* CommonCompressedVideoPacket::queryInterface(const nxpl::NX_GUID& interfaceId)
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

} // namespace metadata
} // namespace sdk
} // namespace nx
