
#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/fusion/fusion_fwd.h>
#include "utils/common/request_param.h"

struct CloudCredentialsData
{
    // TODO: rename it to cloudSystemId
    QString cloudSystemID; //< keep same name as the constant QnGlobalSettings::kNameCloudSystemID
    QString cloudAuthKey; //< keep same name as the constant QnGlobalSettings::kNameCloudAuthKey
    QString cloudAccountName; //< keep same name as the constant QnGlobalSetttings::kNameCloudAccountName

    CloudCredentialsData();
    CloudCredentialsData(const QnRequestParams& params);
};

#define CloudCredentialsData_Fields (cloudSystemID)(cloudAuthKey)(cloudAccountName)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((CloudCredentialsData), (json))
