#pragma once

#include <api/model/password_data.h>

#include <nx/fusion/model_functions_fwd.h>

struct DetachFromCloudData: public PasswordData
{
    DetachFromCloudData();
    DetachFromCloudData(const PasswordData& source);
    DetachFromCloudData(const QnRequestParams& params);
};

#define DetachFromCloudData_Fields PasswordData_Fields

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((DetachFromCloudData), (json))
