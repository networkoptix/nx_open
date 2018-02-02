#pragma once

#include <plugins/plugin_api.h>
#include <nx/sdk/metadata/metadata_packet.h>
#include <nx/sdk/metadata/metadata_item.h>

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Each class that implements IterableMetadataPacket interface
 * should properly handle this GUID in its queryInterface method.
 */
static const nxpl::NX_GUID IID_IterableMetadataPacket
    = {{0x84, 0xbd, 0x34, 0x44, 0x98, 0xa9, 0x40, 0xbc, 0x94, 0x96, 0xec, 0xd3, 0xc9, 0x11, 0x86, 0xe8}};

/**
 * @brief The IterableMetadataPacket class is an interface for
 * metadata packets which consist of multiple objects of the same type
 */
class IterableMetadataPacket: public MetadataPacket
{
public:
    virtual MetadataItem* nextItem() = 0;
};

} // namespace metadata
} // namesapce sdk
} // namespace nx
