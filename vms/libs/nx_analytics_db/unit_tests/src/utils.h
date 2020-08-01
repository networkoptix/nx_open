#pragma once

#include <analytics/db/analytics_db_types.h>
#include <analytics/db/config.h>
#include <utils/math/math.h>

namespace nx::analytics::db {

inline bool operator==(const ObjectPosition& left, const ObjectPosition& right)
{
    return left.deviceId == right.deviceId
        && left.timestampUs == right.timestampUs
        && left.durationUs == right.durationUs
        // NOTE: not checking attributes because they are not saved in the track:
        // this functionality is completely refactored in 4.1.
        //&& left.attributes == right.attributes
        && equalWithPrecision(left.boundingBox, right.boundingBox, kCoordinateDecimalDigits);
}

inline bool operator==(const ObjectTrack& left, const ObjectTrack& right)
{
    const auto result = left.id == right.id
        && left.objectTypeId == right.objectTypeId
        // See note above.
        //&& left.attributes == right.attributes
        && left.firstAppearanceTimeUs == right.firstAppearanceTimeUs
        && left.lastAppearanceTimeUs == right.lastAppearanceTimeUs
        && left.objectPosition == right.objectPosition
        && left.bestShot == right.bestShot;
    return result;
}

} // nx::analytics::db
