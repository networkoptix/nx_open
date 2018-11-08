#include "setup_cloud_system_data.h"

#include <nx/fusion/model_functions.h>

namespace {
static const QString kSystemNameParamName(QLatin1String("systemName"));
}

SetupCloudSystemData::SetupCloudSystemData()
{
}

SetupCloudSystemData::SetupCloudSystemData(const QnRequestParams& params):
    CloudCredentialsData(params),
    systemName(params.value(kSystemNameParamName))
{
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SetupCloudSystemData),
    (json),
    _Fields)
