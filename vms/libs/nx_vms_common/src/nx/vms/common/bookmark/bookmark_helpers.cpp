// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_helpers.h"

#include <chrono>

#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/math/math.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <utils/common/synctime.h>

using namespace std::chrono;

using namespace nx;

namespace {

bool isChangedEnough(milliseconds first, milliseconds second, milliseconds minDiff)
{
    return (std::chrono::abs(first - second) >= minDiff);
}

} // namespace

namespace nx::vms::common {

QnCameraBookmarkList bookmarksAtPosition(
    const QnCameraBookmarkList& bookmarks, qint64 posMs)
{
    const auto predicate =
        [posMs](const QnCameraBookmark &bookmark)
        {
            return qBetween<qint64>(bookmark.startTimeMs.count(), posMs, bookmark.endTime().count());
        };

    QnCameraBookmarkList result;
    std::copy_if(bookmarks.cbegin(), bookmarks.cend(), std::back_inserter(result), predicate);
    return result;
};

bool isTimeWindowChanged(
    milliseconds firstStartTime,
    milliseconds firstEndTime,
    milliseconds secondStartTime,
    milliseconds secondEndTime,
    milliseconds minStep)
{
    return (isChangedEnough(firstStartTime, secondStartTime, minStep)
        || isChangedEnough(firstEndTime, secondEndTime, minStep));
}

QnTimePeriod extendTimeWindow(qint64 startTimeMs,
    qint64 endTimeMs,
    qint64 startTimeShiftMs,
    qint64 endTimeShiftMs)
{
    startTimeMs = startTimeMs - startTimeShiftMs > QnTimePeriod::kMinTimeValue
        ? startTimeMs - startTimeShiftMs
        : QnTimePeriod::kMinTimeValue;

    endTimeMs = QnTimePeriod::kMaxTimeValue - endTimeShiftMs < endTimeMs
        ? endTimeMs + endTimeShiftMs
        : QnTimePeriod::kMaxTimeValue;

    return QnTimePeriod::fromInterval(startTimeMs, endTimeMs);
}

} // namespace nx::vms::common
