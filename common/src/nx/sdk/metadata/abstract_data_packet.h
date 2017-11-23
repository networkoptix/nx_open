#pragma once

#include <plugins/plugin_api.h>

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Each class that implements AbstractDataPacket interface should properly handle this GUID in its
 * queryInterface() method.
 */
static const nxpl::NX_GUID IID_DataPacket
    = {{0x13, 0x85, 0x3c, 0xd6, 0x13, 0x7e, 0x4d, 0x8b, 0x9b, 0x8e, 0x63, 0xf1, 0x5f, 0x93, 0x2a, 0xc1}};

/**
 * Base class for every class that represents the packet of the data (e.g. audio, video, metadata).
 */
class AbstractDataPacket: public nxpl::PluginInterface
{
};

} // namespace metadata
} // namespace sdk
} // namespace nx
