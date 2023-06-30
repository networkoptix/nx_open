// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/event/event_fwd.h>
#include <recording/time_period.h>

class QnCommonModule;

namespace nx::vms::common {

NX_VMS_COMMON_API QnCameraBookmark bookmarkFromAction(
    const nx::vms::event::AbstractActionPtr& action,
    const QnSecurityCamResourcePtr& camera);

NX_VMS_COMMON_API QnCameraBookmarkList bookmarksAtPosition(
    const QnCameraBookmarkList& bookmarks,
    qint64 posMs);

NX_VMS_COMMON_API bool isTimeWindowChanged(
    std::chrono::milliseconds firstStartTime,
    std::chrono::milliseconds firstEndTime,
    std::chrono::milliseconds secondStartTime,
    std::chrono::milliseconds secondEndTime,
    std::chrono::milliseconds minStep);

NX_VMS_COMMON_API QnTimePeriod extendTimeWindow(qint64 startTimeMs,
    qint64 endTimeMs,
    qint64 startTimeShiftMs,
    qint64 endTimeShiftMs);

} // namespace nx::vms::common
