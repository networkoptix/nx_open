// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/data/camera_attributes_data.h>

namespace nx::vms::common {

constexpr auto kDaysInWeek = 7;
constexpr auto kHoursInDay = 24;

/**
* @return Array with all Qt::DayOfWeek enumeration values to be able iterate over them without
*     type casting. No other purposes implied.
*/
NX_VMS_COMMON_API const std::array<Qt::DayOfWeek, kDaysInWeek>& daysOfWeek();

NX_VMS_COMMON_API nx::vms::api::ScheduleTaskDataList scheduleFromByteArray(
    const QByteArray& schedule);

NX_VMS_COMMON_API QByteArray scheduleToByteArray(
    const nx::vms::api::ScheduleTaskDataList& schedule);

NX_VMS_COMMON_API bool timeInSchedule(QDateTime datetime, const QByteArray& schedule);

NX_VMS_COMMON_API nx::vms::api::CameraScheduleTaskDataList defaultSchedule(int fps);

} // namespace nx::vms::common
