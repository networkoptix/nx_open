// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_track_facade.h"

namespace nx::vms::client::core::detail {

ObjectTrackFacade::TimeType ObjectTrackFacade::startTime(const Type& track)
{
    return TimeType(track.firstAppearanceTimeUs);
}

const QnUuid& ObjectTrackFacade::id(const Type& data)
{
    return data.id;
}

bool ObjectTrackFacade::equal(const Type& /*left*/, const Type& /*right*/)
{
    return false; //< Analytics data is unchangable.
}

} // namespace nx::vms::client::core::detail
