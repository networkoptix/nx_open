
#pragma once

#include <utils/common/model_functions_fwd.h>
#include <utils/fusion/fusion_fwd.h>


struct CloudCredentialsData
{
    /** if \a true, credentials are set to empty string */
    bool reset;
    QString cloudSystemId;
    QString cloudAuthenticationKey;

    CloudCredentialsData()
    :
        reset(false)
    {
    }
};

#define CloudCredentialsData_Fields (reset)(cloudSystemId)(cloudAuthenticationKey)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (CloudCredentialsData),
    (json));
