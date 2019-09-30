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
    static auto interfaceId() { return makeId("nx::sdk::analytics::IObjectTrackBestShotPacket"); }

    /**
     * Timestamp of the frame (in microseconds) the best shot belongs to. Should be a non-negative
     * number.
     */
    virtual int64_t timestampUs() const override = 0;

    /** Id of the track the best shot belongs to. */
    protected: virtual void getTrackId(Uuid* outValue) const = 0;
    public: Uuid trackId() const { Uuid value; getTrackId(&value); return value; }

    /**
     * Bounding box of the best shot. If the rectangle returned by this method is invalid then
     * a bounding box from the frame with the timestamp equal to the timestampUs() will be used.
     */
    protected: virtual void getBoundingBox(Rect* outValue) const = 0;
    public: Rect boundingBox() const { Rect value; getBoundingBox(&value); return value; }
};

} // namespace analytics
} // namespace sdk
} // namespace nx
