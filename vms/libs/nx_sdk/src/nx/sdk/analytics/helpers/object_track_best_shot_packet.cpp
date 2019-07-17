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
    m_objectTrackId(trackId),
    m_timestampUs(timestampUs),
    m_boundingBox(boundingBox)
{
}

Uuid ObjectTrackBestShotPacket::trackId() const
{
    return m_objectTrackId;
}

int64_t ObjectTrackBestShotPacket::timestampUs() const
{
    return m_timestampUs;
}

Rect ObjectTrackBestShotPacket::boundingBox() const
{
    return m_boundingBox;
}

} // namespace analytics
} // namespace sdk
} // namespace nx
