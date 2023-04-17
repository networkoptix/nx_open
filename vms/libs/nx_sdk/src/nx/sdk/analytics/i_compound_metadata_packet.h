// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <stdint.h>

#include <nx/sdk/interface.h>

#include "i_metadata.h"
#include "i_metadata_packet.h"

namespace nx::sdk::analytics {

/**
 * Packet containing metadata (e.g. events, object detections).
 */
class ICompoundMetadataPacket: public Interface<ICompoundMetadataPacket, IMetadataPacket0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::ICompoundMetadataPacket"); }

    /**
     * @return Validity duration of the metadata in the packet, or 0 if irrelevant.
     */
    virtual int64_t durationUs() const = 0;

    /** @return Number of elements in the packet. */
    virtual int count() const = 0;

    /** Called by at() */
    protected: virtual const IMetadata* getAt(int index) const = 0;
    /**
     * @return Element at the zero-based index, or null if the index is invalid.
     */
    public: Ptr<const IMetadata> at(int index) const { return Ptr(getAt(index)); }
};
using ICompoundMetadataPacket0 = ICompoundMetadataPacket;

} // namespace nx::sdk::analytics
