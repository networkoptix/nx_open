// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

#include <nx/sdk/analytics/i_metadata_packet.h>
#include <nx/sdk/analytics/rect.h>

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Packet containing information about object track best shot.
 */
class IObjectTrackBestShotPacket: public Interface<IObjectTrackBestShotPacket, IMetadataPacket>
{
public:
    static auto interfaceId()
    {
        return InterfaceId("nx::sdk::analytics::IObjectTrackBestShotPacket");
    }

    /**
     * Timestamp of the frame (in microseconds) the best shot belongs to. Should be a non-negative
     * number.
     */
    virtual int64_t timestampUs() const override = 0;

    // TODO: #mshevchenko: Uuid
    /** Id of the track the best shot belongs to. */
    virtual Uuid trackId() const = 0;

    // TODO: #mshevchenko: Rect
    /**
     * Bounding box of the best shot. If the rectangle returned by this method is invalid then
     * a bounding box from the frame with the timestamp equal to the timestampUs() will be used.
     */
    virtual Rect boundingBox() const = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
