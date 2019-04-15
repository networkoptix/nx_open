#pragma once

#include <api/model/password_data.h>

#include <nx/fusion/model_functions_fwd.h>

struct DetachFromCloudData: public PasswordData, public CurrentPasswordData
{
    DetachFromCloudData(
        const PasswordData& source = {},
        const CurrentPasswordData& currentPassword = {});
};

#define DetachFromCloudData_Fields PasswordData_Fields CurrentPasswordData_Fields

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((DetachFromCloudData), (json))
