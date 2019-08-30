#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/url.h>

namespace nx::cloud::storage::service::api {

struct NX_CLOUD_STORAGE_CLIENT_API StorageCredentials
{
    std::vector<nx::utils::Url> urls;
    std::string login;
    std::string password;
    std::string sessionToken;
    int durationSeconds = 0;
};

#define StorageCredentials_Fields (urls)(login)(password)(sessionToken)(durationSeconds)
QN_FUSION_DECLARE_FUNCTIONS(StorageCredentials, (json), NX_CLOUD_STORAGE_CLIENT_API)

} // namespace nx::cloud::storage::service::api