#pragma once

#include <cstdint>

#include <nx/sdk/interface.h>

#include "i_data_packet.h"
#include "i_metadata.h"

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

    /**
     * @return Validity duration of the metadata in the packet, or 0 if irrelevant.
     */
    virtual int64_t durationUs() const = 0;

    /** @return Number of elements in the packet. */
    virtual int count() const = 0;

    /** @return Element at the zero-based index, or null if the index is invalid. */
    virtual const IMetadata* at(int index) const = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
