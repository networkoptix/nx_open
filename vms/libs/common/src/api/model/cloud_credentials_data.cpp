
#include "cloud_credentials_data.h"

#include <common/common_globals.h>
#include <nx/fusion/model_functions.h>


CloudCredentialsData::CloudCredentialsData()
{
}

CloudCredentialsData::CloudCredentialsData(const QnRequestParams& params):
    cloudSystemID(params.value(lit("cloudSystemID"))), // TODO: rename it to cloudSystemId
    cloudAuthKey(params.value(lit("cloudAuthKey"))),
    cloudAccountName(params.value(lit("cloudAccountName")))
{
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((CloudCredentialsData), (json), _Fields);
