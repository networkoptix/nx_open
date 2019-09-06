#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx::cloud::storage::service::api {

struct NX_CLOUD_STORAGE_CLIENT_API AddSystemRequest
{
    // The id of the system to add
    std::string id;
};

#define AddSystemRequest_Fields (id)

QN_FUSION_DECLARE_FUNCTIONS(AddSystemRequest, (json), NX_CLOUD_STORAGE_CLIENT_API)

} // namespace namespace nx::cloud::storage::service::api