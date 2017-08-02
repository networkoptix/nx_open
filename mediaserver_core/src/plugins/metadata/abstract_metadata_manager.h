#pragma once

#include <plugins/plugin_api.h>

#include <plugins/metadata/abstract_data_packet.h>
#include <plugins/metadata/utils.h>

namespace nx {
namespace sdk {
namespace metadata {

static const nxpl::NX_GUID IID_AbstractMetadataHandler =
    {{0x48, 0x5a, 0x23, 0x51, 0x55, 0x73, 0x4f, 0xb5, 0xa9, 0x11, 0xe4, 0xfb, 0x22, 0x87, 0x79, 0x24}};

class AbstractMetadataHandler: nxpl::PluginInterface
{
public:
    virtual void handleMetadata(Error error, AbstractMetadataPacket* metadata) = 0;
};

static const nxpl::NX_GUID IID_AbstractMetadataManager =
    {{0x48, 0x5a, 0x23, 0x51, 0x55, 0x73, 0x4f, 0xb5, 0xa9, 0x11, 0xe4, 0xfb, 0x22, 0x87, 0x79, 0x24}};

class AbstractMetadataManager: nxpl::PluginInterface
{
public:
    virtual ~AbstractMetadataHandler() {}

    /**
     * AbstractMetadataManager will call @param handler when it gets new metadata.
     **/
    virtual Error startFetchingMetadata(AbstractMetadataHandler* handler) = 0;
    virtual Error stopFetchingMetadata() = 0;

    virtual Error capabiltiesManifest(char* buffer, int* inOutBufferSize);
};

} // namespace metadata
} // namespace sdk
} // namespace nx
