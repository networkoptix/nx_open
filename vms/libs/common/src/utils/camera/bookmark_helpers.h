#pragma once

#include <recording/time_period.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <nx/vms/event/event_fwd.h>

class QnCommonModule;

namespace helpers {

QString getBookmarkCreatorName(
    const QnCameraBookmark& bookmark,
    QnResourcePool* resourcePool);

QnCameraBookmark bookmarkFromAction(
    const nx::vms::event::AbstractActionPtr& action,
    const QnSecurityCamResourcePtr& camera);

QnCameraBookmarkList bookmarksAtPosition(const QnCameraBookmarkList& bookmarks,
    qint64 posMs);

bool isTimeWindowChanged(
    std::chrono::milliseconds firstStartTime,
    std::chrono::milliseconds firstEndTime,
    std::chrono::milliseconds secondStartTime,
    std::chrono::milliseconds secondEndTime,
    std::chrono::milliseconds minStep);

QnTimePeriod extendTimeWindow(qint64 startTimeMs,
    qint64 endTimeMs,
    qint64 startTimeShiftMs,
    qint64 endTimeShiftMs);

} // namespace helpers
