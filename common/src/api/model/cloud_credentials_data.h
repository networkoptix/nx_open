
#pragma once

#include <utils/common/model_functions_fwd.h>
#include <utils/fusion/fusion_fwd.h>
#include "utils/common/request_param.h"

struct CloudCredentialsData
{
    /** if \a true, credentials are set to empty string */
    bool reset;
    QString cloudSystemID; //< keep same name as the constant CLOUD_SYSTEM_ID
    QString cloudAuthKey; //< keep same name as the constant CLOUD_SYSTEM_AUTH_KEY
    QString cloudAccountName; //< keep same name as the constant CLOUD_ACCOUNT_NAME
    
    CloudCredentialsData(const QnRequestParams& params);

    CloudCredentialsData()
    :
        reset(false)
    {
    }
};

#define CloudCredentialsData_Fields (reset)(cloudSystemID)(cloudAuthKey)(cloudAccountName)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (CloudCredentialsData),
    (json));
