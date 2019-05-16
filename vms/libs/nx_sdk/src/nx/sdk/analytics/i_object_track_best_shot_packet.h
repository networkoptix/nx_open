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
     * Timestamp of the frame (in microseconds) the best shot belongs to. Should be a positive
     * number.
     */
    virtual int64_t timestampUs() const = 0;

    /** Id of the track the best shot belongs to. */
    virtual nx::sdk::Uuid trackId() const = 0;

    /**
     * Bounding box of the best shot. If the rectangle returned by this method is invalid then
     * a bounding box from the frame with the timestamp equal to the timestampUs() will be used.
     */
    virtual Rect boundingBox() const = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx