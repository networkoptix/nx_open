#pragma once

#include <api/model/password_data.h>

#include <nx/fusion/model_functions_fwd.h>

struct DetachFromCloudData: public PasswordData, public CurrentPasswordData
{
    DetachFromCloudData();
    DetachFromCloudData(const PasswordData& source, const CurrentPasswordData& currentPassword);
    DetachFromCloudData(const QnRequestParams& params);
};

#define DetachFromCloudData_Fields PasswordData_Fields CurrentPasswordData_Fields

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((DetachFromCloudData), (json))
