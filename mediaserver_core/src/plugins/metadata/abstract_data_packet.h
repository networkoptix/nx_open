#pragma once

#include <plugins/plugin_api.h>

namespace nx {
namespace sdk {
namespace metadata {

static const nxpl::GUID IID_DataPacket
    = {{0x13, 0x85, 0x3c, 0xd6, 0x13, 0x7e, 0x4d, 0x8b, 0x9b, 0x8e, 0x63, 0xf1, 0x5f, 0x93, 0x2a, 0xc1}};

class AbstractDataPacket: public nxpl::PluginInterface
{

};

} // namespace metadata
} // namespace sdk
} // namespace nx
