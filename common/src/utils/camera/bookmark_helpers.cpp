
#include "bookmark_helpers.h"

#include <utils/math/math.h>
#include <core/resource/camera_bookmark.h>

namespace
{
    bool isChangedEnough(qint64 first
        , qint64 second
        , qint64 minDiff)
    {
        return (std::abs(first - second) >= minDiff);
    }
}

QnCameraBookmarkList helpers::bookmarksAtPosition(const QnCameraBookmarkList &bookmarks
                                         , qint64 posMs)
{
    const auto predicate = [posMs](const QnCameraBookmark &bookmark)
    {
        return qBetween(bookmark.startTimeMs, posMs, bookmark.endTimeMs());
    };

    QnCameraBookmarkList result;
    std::copy_if(bookmarks.cbegin(), bookmarks.cend(), std::back_inserter(result), predicate);
    return result;
};

bool helpers::isTimeWindowChanged(qint64 firstStartTimeMs
    , qint64 firstEndTimeMs
    , qint64 secondStartTimeMs
    , qint64 secondEndTimeMs
    , qint64 minStep)
{
    return (isChangedEnough(firstStartTimeMs, secondStartTimeMs, minStep)
        || isChangedEnough(firstEndTimeMs, secondEndTimeMs, minStep));
}

QnTimePeriod helpers::extendTimeWindow(qint64 startTimeMs
    , qint64 endTimeMs
    , qint64 startTimeShiftMs
    , qint64 endTimeShiftMs)
{
    startTimeMs = (startTimeMs > startTimeShiftMs
        ? startTimeMs - startTimeShiftMs : QnTimePeriod::kMinTimeValue);

    endTimeMs = (QnTimePeriod::kMaxTimeValue - endTimeShiftMs > endTimeMs
        ? endTimeMs + endTimeShiftMs : QnTimePeriod::kMaxTimeValue);

    return QnTimePeriod::fromInterval(startTimeMs, endTimeMs);
}
