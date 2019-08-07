// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

#include "i_object_metadata.h"
#include "i_compound_metadata_packet.h"

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Metadata packet that contains data about objects detected on the scene.
 */
class IObjectMetadataPacket: public Interface<IObjectMetadataPacket, ICompoundMetadataPacket>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::analytics::IObjectMetadataPacket"); }

    protected: virtual const IObjectMetadata* getAt(int index) const override = 0;
    public: Ptr<const IObjectMetadata> at(int index) const { return toPtr(getAt(index)); }
};

} // namespace analytics
} // namespace sdk
} // namespace nx
