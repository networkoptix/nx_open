// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_helpers.h"

#include <chrono>

#include <core/resource/camera_bookmark.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/math/math.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/strings_helper.h>
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

QnCameraBookmark bookmarkFromAction(
    const vms::event::AbstractActionPtr& action,
    const QnSecurityCamResourcePtr& camera)
{
    if (!camera || !camera->systemContext())
    {
        NX_ASSERT(false, "Camera is invalid");
        return QnCameraBookmark();
    }

    const auto actionParams = action->getParams();
    const qint64 recordBeforeMs = actionParams.recordBeforeMs;
    const qint64 recordAfterMs = actionParams.recordAfter;
    const qint64 fixedDurationMs = actionParams.durationMs;

    const auto runtimeParams = action->getRuntimeParams();
    const qint64 startTimeMs = runtimeParams.eventTimestampUsec / 1000;
    const qint64 endTimeMs = startTimeMs;

    QnCameraBookmark bookmark;

    bookmark.guid = QnUuid::createUuid();
    bookmark.startTimeMs = milliseconds(startTimeMs - recordBeforeMs);
    bookmark.durationMs = milliseconds(fixedDurationMs > 0
        ? fixedDurationMs
        : endTimeMs - startTimeMs);
    bookmark.durationMs += milliseconds(recordBeforeMs + recordAfterMs);
    bookmark.cameraId = camera->getId();
    bookmark.creationTimeStampMs = qnSyncTime->value();

    vms::event::StringsHelper helper(camera->systemContext());
    bookmark.name = helper.eventAtResource(action->getRuntimeParams(), Qn::RI_WithUrl);
    bookmark.description = helper.eventDetails(action->getRuntimeParams(),
        nx::vms::event::AttrSerializePolicy::singleLine).join('\n');
    bookmark.tags = nx::utils::toQSet(
        action->getParams().tags.split(',', Qt::SkipEmptyParts));
    return bookmark;
}

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
