#pragma once

#include <vector>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/analytics/i_object_track_best_shot_packet.h>

namespace nx {
namespace sdk {
namespace analytics {

class ObjectTrackBestShotPacket: public RefCountable<IObjectTrackBestShotPacket>
{
public:
    ObjectTrackBestShotPacket(Uuid trackId, int64_t timestampUs, Rect boundingBox = Rect());

    virtual Uuid trackId() const override;

    virtual int64_t timestampUs() const override;

    virtual Rect boundingBox() const override;

private:
    Uuid m_trackId;
    int64_t m_timestampUs = 0;
    Rect m_boundingBox;
};

} // namespace analytics
} // namespace sdk
} // namespace nx