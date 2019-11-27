// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

#include <nx/sdk/i_list.h>
#include <nx/sdk/analytics/i_metadata_packet.h>
#include <nx/sdk/analytics/i_uncompressed_video_frame_1.h>

namespace nx {
namespace sdk {
namespace analytics {

class IUncompressedVideoFrame: public Interface<IUncompressedVideoFrame, IUncompressedVideoFrame1>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IUncompressedVideoFrame1"); }

protected: virtual IList<IMetadataPacket>* getMetadataList() const = 0;
public: Ptr<IList<IMetadataPacket>> metadataList() const
{
    return toPtr(getMetadataList());
}

};

} // namespace analytics
} // namespace sdk
} // namespace nx
