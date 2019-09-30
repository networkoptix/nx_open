// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/analytics/i_object_track_best_shot_packet.h>

namespace nx {
namespace sdk {
namespace analytics {

class ObjectTrackBestShotPacket: public RefCountable<IObjectTrackBestShotPacket>
{
public:
    ObjectTrackBestShotPacket(Uuid trackId, int64_t timestampUs, Rect boundingBox = Rect());

    virtual int64_t timestampUs() const override;

protected:
    virtual void getTrackId(Uuid* outValue) const override;
    virtual void getBoundingBox(Rect* outValue) const override;

private:
    Uuid m_trackId;
    int64_t m_timestampUs = -1;
    Rect m_boundingBox;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
