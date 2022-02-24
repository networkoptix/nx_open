// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/model/password_data.h>

#include <nx/fusion/model_functions_fwd.h>

struct NX_VMS_COMMON_API DetachFromCloudData:
    public PasswordData,
    public CurrentPasswordData
{
    DetachFromCloudData(
        const PasswordData& source = {},
        const CurrentPasswordData& currentPassword = {});
};

#define DetachFromCloudData_Fields PasswordData_Fields CurrentPasswordData_Fields

QN_FUSION_DECLARE_FUNCTIONS(DetachFromCloudData, (json), NX_VMS_COMMON_API)
