// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_track_facade.h"

#include <nx/utils/log/log_main.h>

namespace nx::vms::client::core::detail {

ObjectTrackFacade::TimeType ObjectTrackFacade::startTime(const Type& track)
{
    return TimeType(track.firstAppearanceTimeUs);
}

nx::Uuid ObjectTrackFacade::id(const Type& data)
{
    if (!data.id.isNull())
        return data.id;

    NX_WARNING(NX_SCOPE_TAG, "Object track can't have empty id, generating it from the data.");
    return nx::Uuid::fromArbitraryData(data.analyticsEngineId.toString()
        + data.deviceId.toString()
        + QString::number(data.firstAppearanceTimeUs));
}

bool ObjectTrackFacade::equal(const Type& left, const Type& right)
{
    return id(left) == id(right)
        && startTime(left) == startTime(right);
}

} // namespace nx::vms::client::core::detail
