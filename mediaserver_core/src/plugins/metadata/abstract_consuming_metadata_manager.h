#pragma once

#include <plugins/metadata/abstract_metadata_manager.h>
#include <plugins/metadata/abstract_data_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

static const nxpl::NX_GUID IID_ConsumingMetadataManager =
    {{0xb6, 0x7b, 0xce, 0x8c, 0x67, 0x68, 0x4e, 0x9b, 0xbd, 0x56, 0x8c, 0x02, 0xca, 0x0a, 0x1b, 0x18}};

class AbstractConsumingMetadataManager: public AbstractMetadataManager
{
public:
    virtual Error pushData(AbstractDataPacket* dataPacket) = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
