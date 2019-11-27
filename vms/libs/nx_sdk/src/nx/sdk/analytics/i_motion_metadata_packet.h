// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

#include <nx/sdk/analytics/i_motion_metadata.h>
#include <nx/sdk/analytics/i_compound_metadata_packet.h>

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Metadata packet containing information about motion on the scene. Typically contains a single
 *     IMotionMetadata object.
 */
class IMotionMetadataPacket: public Interface<IMotionMetadataPacket, ICompoundMetadataPacket>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IMotionMetadataPacket"); }

protected: virtual IMotionMetadata* getAt(int index) const override = 0;
public: Ptr<const IMotionMetadata> at(int index) const { return toPtr(getAt(index)); }
};

} // namespace analytics
} // namespace sdk
} // namespace nx
