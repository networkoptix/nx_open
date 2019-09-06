// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_track_best_shot_packet.h"

namespace nx {
namespace sdk {
namespace analytics {

ObjectTrackBestShotPacket::ObjectTrackBestShotPacket(
    Uuid trackId,
    int64_t timestampUs,
    Rect boundingBox)
    :
    m_trackId(trackId),
    m_timestampUs(timestampUs),
    m_boundingBox(boundingBox)
{
}

void ObjectTrackBestShotPacket::getTrackId(Uuid* outValue) const
{
    *outValue = m_trackId;
}

int64_t ObjectTrackBestShotPacket::timestampUs() const
{
    return m_timestampUs;
}

void ObjectTrackBestShotPacket::getBoundingBox(Rect* outValue) const
{
    *outValue = m_boundingBox;
}

} // namespace analytics
} // namespace sdk
} // namespace nx
