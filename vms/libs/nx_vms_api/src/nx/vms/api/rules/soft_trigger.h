// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

#include "../data/schedule_task_data.h"
#include "common.h"
#include "field_types.h"

namespace nx::vms::api::rules {

struct NX_VMS_API SoftTriggerData
{
    /**%apidoc ID of the soft trigger to execute. */
    nx::Uuid triggerId;

    /**%apidoc ID of the Device assigned to the soft trigger. */
    nx::Uuid deviceId;

    /**%apidoc[opt] State of the soft trigger. */
    State state = State::instant;
};

#define SoftTriggerData_Fields (triggerId)(deviceId)(state)
QN_FUSION_DECLARE_FUNCTIONS(SoftTriggerData, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(SoftTriggerData, SoftTriggerData_Fields)

struct SoftTriggerInfo
{
    /**%apidoc ID of the soft trigger. */
    nx::Uuid triggerId;

    /**%apidoc Flag indicating soft trigger should start and stop. */
    bool prolonged = false;

    /**%apidoc Devices assigned to the soft trigger. */
    UuidSelection devices;

    /**%apidoc Name of the soft trigger. */
    QString name;

    /**%apidoc Icon of the soft trigger. */
    QString icon;

    /**%apidoc[opt] Schedule of the soft trigger rule. */
    nx::vms::api::ScheduleTaskDataList schedule;
};

using SoftTriggerInfoList = std::vector<SoftTriggerInfo>;

#define SoftTriggerInfo_Fields (triggerId)(prolonged)(devices)(name)(icon)(schedule)
QN_FUSION_DECLARE_FUNCTIONS(SoftTriggerInfo, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(SoftTriggerInfo, SoftTriggerInfo_Fields)

} // namespace nx::vms::api::rules
