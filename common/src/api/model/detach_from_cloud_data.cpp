#include "detach_from_cloud_data.h"

#include <nx/fusion/model_functions.h>

DetachFromCloudData::DetachFromCloudData():
    PasswordData()
{
}

DetachFromCloudData::DetachFromCloudData(const PasswordData& source):
    PasswordData(source)
{
}

DetachFromCloudData::DetachFromCloudData(const QnRequestParams& params):
    PasswordData(params)
{
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((DetachFromCloudData), (json), _Fields)
