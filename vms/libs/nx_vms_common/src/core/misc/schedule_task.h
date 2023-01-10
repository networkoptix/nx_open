// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/vms/api/data/camera_attributes_data.h>

using QnScheduleTask = nx::vms::api::ScheduleTaskData;
using QnScheduleTaskList = std::vector<QnScheduleTask>;

NX_VMS_COMMON_API QnScheduleTaskList defaultSchedule(int fps);
