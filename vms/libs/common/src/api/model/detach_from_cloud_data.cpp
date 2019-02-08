#include "detach_from_cloud_data.h"

#include <nx/fusion/model_functions.h>

DetachFromCloudData::DetachFromCloudData():
    PasswordData()
{
}

DetachFromCloudData::DetachFromCloudData(
        const PasswordData& source, const CurrentPasswordData& currentPassword):
    PasswordData(source),
    CurrentPasswordData(currentPassword)
{
}

DetachFromCloudData::DetachFromCloudData(const QnRequestParams& params):
    PasswordData(params),
    CurrentPasswordData(params)
{
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((DetachFromCloudData), (json), _Fields)
