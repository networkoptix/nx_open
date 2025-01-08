// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "deployment_code_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    DeploymentCodeRequestData,
    (json),
    DeploymentCodeRequestData_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    DeploymentCodeData,
    (json),
    DeploymentCodeData_Fields)

} // namespace nx::vms::api
