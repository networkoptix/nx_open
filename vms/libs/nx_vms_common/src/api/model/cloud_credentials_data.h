// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/fusion/model_functions_fwd.h>

struct CloudCredentialsData
{
    // Keep same names as the constants in nx::vms::common::SystemSettings::Names.
    QString cloudSystemID;
    QString cloudAuthKey;
    QString cloudAccountName;
    QString organizationId;
};

#define CloudCredentialsData_Fields (cloudSystemID)(cloudAuthKey)(cloudAccountName)(organizationId)

QN_FUSION_DECLARE_FUNCTIONS(CloudCredentialsData, (json), NX_VMS_COMMON_API)
