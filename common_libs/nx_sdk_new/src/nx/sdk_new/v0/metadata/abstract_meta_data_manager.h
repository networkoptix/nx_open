#pragma once

#include <nx/sdk_new/v0/common/abstract_object.h>
#include <nx/sdk_new/v0/common/abstract_data_packet.h>
#include <nx/sdk_new/v0/common/abstract_meta_data_packet.h>

namespace nx {
namespace sdk {
namespace v0 {
namespace metadata {

using MetadataReadyCallback = void (*)(AbstractMetaDataPacket* packet);

using ErrorCallback = void (*)(); //< TODO: #dmishin think about its signature.

class AbstractMetaDataManager: public AbstractObject
{
    virtual ~AbstractMetaDataManager() {}

    virtual CapabilityManifest capabiltyManifest() const = 0;

    virtual void setMetaDataCallback(
        nx::sdk::v0::metadata::MetaDataReadyCallback callback) = 0;

    virtual void setErrorCallback(
        nx::sdk::v0::metadata::ErrorCallback callback) = 0;

    virtual bool pushData(const nx::sdk::v0::AbstractDataPacket* packet) = 0;

    virtual bool start() = 0;

    virtual bool stop() = 0;
};

} // namesapce metadata
} // namespace v0
} // namespace sdk
} // namespace nx
