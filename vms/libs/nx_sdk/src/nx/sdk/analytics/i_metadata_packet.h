// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/i_data_packet.h>
#include <nx/sdk/interface.h>

namespace nx::sdk::analytics {

/**
 * Packet containing metadata (e.g. events, object detections).
 */
class IMetadataPacket: public Interface<IMetadataPacket, IDataPacket0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IMetadataPacket"); }
};
using IMetadataPacket0 = IMetadataPacket;

} // namespace nx::sdk::analytics
