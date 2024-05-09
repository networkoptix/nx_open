// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rest_api_versions.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

RestApiVersions RestApiVersions::current()
{
    return {std::string(kRestApiVersions.front()), std::string(kRestApiVersions.back())};
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(RestApiVersions, (json), RestApiVersions_Fields)

} // namespace nx::vms::api
