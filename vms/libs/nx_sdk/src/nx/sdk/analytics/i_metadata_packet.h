// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    static auto interfaceId() { return makeId("nx::sdk::analytics::IMetadataPacket"); }
};

} // namespace analytics
} // namespace sdk
} // namespace nx
