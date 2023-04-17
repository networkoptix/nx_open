// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

#include "i_compound_metadata_packet.h"
#include "i_object_metadata.h"

namespace nx::sdk::analytics {

/**
 * Metadata packet that contains data about objects detected on the scene.
 */
class IObjectMetadataPacket0: public Interface<IObjectMetadataPacket0, ICompoundMetadataPacket0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IObjectMetadataPacket"); }

    /** Called by at() */
    protected: virtual const IObjectMetadata* getAt(int index) const override = 0;
    public: Ptr<const IObjectMetadata> at(int index) const { return Ptr(getAt(index)); }
};

class IObjectMetadataPacket: public Interface<IObjectMetadataPacket, IObjectMetadataPacket0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IObjectMetadataPacket1"); }

    virtual Flags flags() const = 0;
};
using IObjectMetadataPacket1 = IObjectMetadataPacket;

} // namespace nx::sdk::analytics
