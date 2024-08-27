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
    // TODO: #spanasenko and-or logic
    QList<EventFilter> eventList;
    QList<ActionBuilder> actionList;

    /**%apidoc[opt] Is rule currently enabled. */
    bool enabled = true;
    /**%apidoc[unused] */
    bool internal = false;

    /**%apidoc[opt] Schedule of the rule. Empty list means the rule is always enabled. */
    nx::vms::api::ScheduleTaskDataList schedule;

    /**%apidoc[opt] String comment explaining the rule. */
    QString comment;
};

#define nx_vms_api_rules_Rule_Fields \
    IdData_Fields(eventList)(actionList)(enabled)(internal)(schedule)(comment)
NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(Rule, (json)(ubjson))

// A dummy struct used in ec2 transactions.
struct NX_VMS_API ResetRules
{
    /**%apidoc[opt] Reset to default rule set if true, clear rules if false. */
    bool useDefault = true;
};

#define nx_vms_api_rules_ResetRules_Fields (useDefault)
NX_VMS_API_DECLARE_STRUCT_EX(ResetRules, (json)(ubjson))

} // namespace nx::vms::api::rules
