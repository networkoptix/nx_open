// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

#include "i_event_metadata.h"
#include "i_compound_metadata_packet.h"

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Metadata packet containing information about some event which occurred on the scene.
 */
class IEventMetadataPacket0: public Interface<IEventMetadataPacket0, ICompoundMetadataPacket0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IEventMetadataPacket"); }

    /** Called by at() */
    protected: virtual const IEventMetadata* getAt(int index) const override = 0;
    public: Ptr<const IEventMetadata> at(int index) const { return toPtr(getAt(index)); }
};

class IEventMetadataPacket: public Interface<IEventMetadataPacket, IEventMetadataPacket0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IEventMetadataPacket1"); }

    virtual Flags flags() const = 0;
};
using IEventMetadataPacket1 = IEventMetadataPacket;

} // namespace analytics
} // namespace sdk
} // namespace nx
