#pragma once

#include <cstdint>

#include <nx/sdk/interface.h>

#include "i_metadata_packet.h"
#include "i_metadata.h"

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Packet containing metadata (e.g. events, object detections).
 */
class ICompoundMetadataPacket: public Interface<ICompoundMetadataPacket, IMetadataPacket>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::analytics::ICompoundMetadataPacket"); }

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
