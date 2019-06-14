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
class IEventMetadataPacket: public Interface<IEventMetadataPacket, ICompoundMetadataPacket>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::analytics::IEventMetadataPacket"); }

    virtual int count() const override = 0;

    virtual const IEventMetadata* at(int index) const override = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
