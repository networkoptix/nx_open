// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    static auto interfaceId() { return makeId("nx::sdk::analytics::ICompoundMetadataPacket"); }

    /**
     * @return Validity duration of the metadata in the packet, or 0 if irrelevant.
     */
    virtual int64_t durationUs() const = 0;

    /** @return Number of elements in the packet. */
    virtual int count() const = 0;

    /**
     * @return Element at the zero-based index, or null if the index is invalid.
     */
    protected: virtual const IMetadata* getAt(int index) const = 0;
    public: Ptr<const IMetadata> at(int index) const { return toPtr(getAt(index)); }
};

} // namespace analytics
} // namespace sdk
} // namespace nx
