// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

#include <nx/sdk/i_list.h>
#include <nx/sdk/analytics/i_metadata_packet.h>
#include <nx/sdk/analytics/i_compressed_video_packet_1.h>

namespace nx {
namespace sdk {
namespace analytics {

class ICompressedVideoPacket: public Interface<ICompressedVideoPacket, ICompressedVideoPacket1>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::ICompressedVideoPacket1"); }

protected: virtual IList<IMetadataPacket>* getMetadataList() const = 0;
public: Ptr<IList<IMetadataPacket>> metadataList() const
{
    return toPtr(getMetadataList());
}

};

} // namespace analytics
} // namespace sdk
} // namespace nx
