// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "schedule_task_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ScheduleTaskData, (ubjson)(xml)(json)(sql_record)(csv_record), ScheduleTaskData_Fields)

} // namespace nx::vms::api
