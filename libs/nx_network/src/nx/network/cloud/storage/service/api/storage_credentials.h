#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/url.h>

namespace nx::cloud::storage::service::api {

struct NX_NETWORK_API StorageCredentials
{
    std::vector<nx::utils::Url> urls;
    std::string login;
    std::string password;
    int durationSeconds = 0;
};

#define StorageCredentials_Fields (urls)(login)(password)(durationSeconds)
QN_FUSION_DECLARE_FUNCTIONS(StorageCredentials, (json), NX_NETWORK_API)

} // namespace nx::cloud::storage::service::api