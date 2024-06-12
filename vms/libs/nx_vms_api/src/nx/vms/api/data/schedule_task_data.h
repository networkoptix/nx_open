// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "data_macros.h"

namespace nx::vms::api {

struct NX_VMS_API ScheduleTaskData
{
    /**%apidoc[opt] Time of day when the task starts (in seconds passed from 00:00:00). */
    int startTime = 0;

    /**%apidoc[opt] Time of day when the task ends (in seconds passed from 00:00:00). */
    int endTime = 0;

    /**%apidoc[opt] Weekday for the recording task.
     *     %value 1 Monday
     *     %value 2 Tuesday
     *     %value 3 Wednesday
     *     %value 4 Thursday
     *     %value 5 Friday
     *     %value 6 Saturday
     *     %value 7 Sunday
     */
    qint8 dayOfWeek = 1;

    inline bool isValid() const { return startTime < endTime; }

    bool operator==(const ScheduleTaskData& other) const = default;
};
#define ScheduleTaskData_Fields (startTime)(endTime)(dayOfWeek)

NX_REFLECTION_INSTRUMENT(ScheduleTaskData, ScheduleTaskData_Fields)

NX_VMS_API_DECLARE_STRUCT_AND_LIST(ScheduleTaskData)

} // namespace nx::vms::api
