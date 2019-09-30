#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx::cloud::storage::service::api {

struct NX_CLOUD_STORAGE_CLIENT_API AddStorageRequest
{
    // Size in bytes of the requested storage
    std::int64_t totalSpace = 0;
    // Optional region. If empty, the region is auto-detected.
    // If non-empty and invalid, error is reported.
    std::string region;
};

#define AddStorageRequest_Fields (totalSpace)(region)

QN_FUSION_DECLARE_FUNCTIONS(AddStorageRequest, (json), NX_CLOUD_STORAGE_CLIENT_API)

} // namespace nx::cloud::storage::service::api