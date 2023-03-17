// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../data/data_macros.h"
#include "../data/id_data.h"
#include "../data/schedule_task_data.h"
#include "action_builder.h"
#include "event_filter.h"

namespace nx::vms::api::rules {

struct NX_VMS_API Rule: IdData
{
    Q_GADGET

public:
    // TODO: #spanasenko and-or logic
    QList<EventFilter> eventList;
    QList<ActionBuilder> actionList;
    bool enabled = true;
    nx::vms::api::ScheduleTaskDataList schedule;
    QString comment;
};

#define nx_vms_api_rules_Rule_Fields \
    IdData_Fields(eventList)(actionList)(enabled)(schedule)(comment)

NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(Rule, (json)(ubjson)(xml))

// A dummy struct used in ec2 transactions.
struct NX_VMS_API ResetRules
{
    bool none = false; //< A dummy field that is necessary for nxfusion serialization.
};

#define nx_vms_api_rules_ResetRules_Fields (none)

NX_VMS_API_DECLARE_STRUCT_EX(ResetRules, (json)(ubjson))

} // namespace nx::vms::api::rules
