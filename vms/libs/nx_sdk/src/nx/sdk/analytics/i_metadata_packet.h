#pragma once

#include <nx/sdk/interface.h>

#include <nx/sdk/analytics/i_data_packet.h>

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Packet containing metadata (e.g. events, object detections).
 */
class IMetadataPacket: public Interface<IMetadataPacket, IDataPacket>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::analytics::IMetadataPacket"); }
};

} // namespace analytics
} // namespace sdk
} // namespace nx
