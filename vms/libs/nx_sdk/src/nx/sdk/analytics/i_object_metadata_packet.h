#pragma once

#include <nx/sdk/interface.h>

#include "i_object_metadata.h"
#include "i_metadata_packet.h"

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Metadata packet that contains data about objects detected on the scene.
 */
class IObjectMetadataPacket: public Interface<IObjectMetadataPacket, IMetadataPacket>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::analytics::IObjectMetadataPacket"); }

    virtual int count() const override = 0;

    virtual const IObjectMetadata* at(int index) const override = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
