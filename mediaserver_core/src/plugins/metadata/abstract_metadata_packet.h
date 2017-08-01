#pragma once

#include <plugins/metadata/abstract_data_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

static const nxpl::NX_GUID IID_MetadataPacket =
    {{0x28, 0xbb, 0xe1, 0x4e, 0xda, 0xea, 0x48, 0xc9, 0xb9, 0x14, 0xfb, 0x07, 0xde, 0x28, 0x6c, 0xbf}};

class AbstractMetadataPacket: public AbstractDataPacket
{

};

} // namespace metadata
} // namespace sdk
} // namespace nx
