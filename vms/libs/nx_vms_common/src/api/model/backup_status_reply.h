// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/vms/api/types/event_rule_types.h>
#include <common/common_globals.h>

struct QnBackupStatusData
{
    Qn::BackupState state = Qn::BackupState_None;
    nx::vms::api::EventReason result = nx::vms::api::EventReason::none;
    qreal progress = 0;

    // TODO: #rvasilenko why start time? Should we automatically add 2 minutes on the client side?
    qint64 backupTimeMs = 0; //< Synchronized utc start time of the last chunk, that was backed up.
};
#define QnBackupStatusData_Fields (state)(result)(progress)(backupTimeMs)

QN_FUSION_DECLARE_FUNCTIONS(QnBackupStatusData, (json), NX_VMS_COMMON_API)
