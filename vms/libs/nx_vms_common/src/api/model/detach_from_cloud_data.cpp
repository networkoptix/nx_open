// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "detach_from_cloud_data.h"

#include <nx/fusion/model_functions.h>

DetachFromCloudData::DetachFromCloudData(
    const PasswordData& source,
    const CurrentPasswordData& currentPassword)
    :
    PasswordData(source),
    CurrentPasswordData(currentPassword)
{
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DetachFromCloudData, (json), DetachFromCloudData_Fields)
