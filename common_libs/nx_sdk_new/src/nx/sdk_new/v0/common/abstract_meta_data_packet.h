#pragma once

#include <nx/sdk_new/v0/common/abstract_data_packet.h>
#include <nx/sdk_new/v0/common/metadata_type.h>

namespace nx {
namespace sdk {
namespace v0 {

class AbstractMetaDataPacket: public AbstractDataPacket
{
    virtual nx::sdk::v0::MetadataType metadataType() const = 0;
};

} // namespace v0
} // namespace sdk
} // namespace nx
