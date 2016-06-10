
#include "cloud_credentials_data.h"

#include <common/common_globals.h>
#include <nx/fusion/model_functions.h>

CloudCredentialsData::CloudCredentialsData(const QnRequestParams& params):
    reset(params.contains(lit("reset"))),
    cloudSystemID(params.value(lit("cloudSystemID"))),
    cloudAuthKey(params.value(lit("cloudAuthKey")))
{
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (CloudCredentialsData),
    (json),
    _Fields,
    (optional, true));
