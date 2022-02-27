// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "action_builder.h"
#include "event_filter.h"

#include <nx/utils/uuid.h>
#include <nx/vms/api/data/data_macros.h>

namespace nx::vms::api::rules {

struct NX_VMS_API Rule
{
    Q_GADGET

public:
    QnUuid id;
    // TODO: #spanasenko and-or logic
    QList<EventFilter> eventList;
    QList<ActionBuilder> actionList;
    bool enabled;
    QByteArray schedule;
    QString comment;
};
#define nx_vms_api_rules_Rule_Fields \
    (id)(eventList)(actionList)(enabled)(schedule)(comment)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(Rule)

// A dummy struct used in ec2 transactions.
struct NX_VMS_API ResetRules
{
    bool none = false; //< A dummy field that is necessary for nxfusion serialization.
};
#define nx_vms_api_rules_ResetRules_Fields (none)
NX_VMS_API_DECLARE_STRUCT(ResetRules)

QN_FUSION_DECLARE_FUNCTIONS(Rule, (json)(ubjson)(csv_record)(xml), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(ResetRules, (json)(ubjson), NX_VMS_API)

} // namespace nx::vms::api::rules
