#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx::cloud::storage::service::api {

struct NX_CLOUD_STORAGE_CLIENT_API System
{
    // The id of this system
    std::string id;
    // The storage id that the system is associated with
    std::string storageId;
};

#define System_Fields (id)(storageId)

QN_FUSION_DECLARE_FUNCTIONS(System, (ubjson)(json), NX_CLOUD_STORAGE_CLIENT_API)

} // namespace nx::cloud::storage::service::api