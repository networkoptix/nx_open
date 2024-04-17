// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::api {

struct NX_VMS_API UpdateCredentialsData
{
    std::string token;
};
#define UpdateCredentialsData_Fields (token)
QN_FUSION_DECLARE_FUNCTIONS(UpdateCredentialsData, (ubjson)(json), NX_VMS_API)

} // namespace nx::vms::api
