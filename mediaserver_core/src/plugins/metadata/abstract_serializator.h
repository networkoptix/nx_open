#pragma once

#include <plugins/plugin_api.h>
#include <plugins/metadata/utils.h>
#include <plugins/metadata/abstract_data_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

static const nxpl::GUID IID_Serializator
    = {{0xf1, 0xc8, 0xfc, 0x64, 0x03, 0x19, 0x47, 0x6f, 0xa9, 0xfd, 0xee, 0x24, 0x32, 0x27, 0xe9, 0xc7}};

class AbstractSerializator: nxpl::PluginInterface
{
    virtual Error serialize(const AbstractDataPacket* packet, char* buffer, int bufferSize) = 0;
    virtual Error deserialize(AbstractDataPacket** packet, const char* buffer, int bufferSize) = 0;
};

} // namespaec metadata
} // namespace sdk
} // namespace nx
