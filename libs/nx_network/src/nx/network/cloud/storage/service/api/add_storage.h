#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx::cloud::storage::service::api {

struct NX_NETWORK_API AddStorageRequest
{
    // Size in bytes of the requested storage
    int size = 0;
};

#define AddStorageRequest_Fields (size)

QN_FUSION_DECLARE_FUNCTIONS(AddStorageRequest, (json), NX_NETWORK_API)

//-------------------------------------------------------------------------------------------------
// AddStorageResponse

struct NX_NETWORK_API AddStorageResponse
{
    // The id of the newly created storage
    std::string id;
    // Total size of the storage in bytes
    int totalSpace = 0;
    // Free space in the storage in bytes
    int freeSpace = 0;
    // List of device ids
    std::vector<std::string> devices;
};

#define AddStorageResponse_Fields (id)(totalSpace)(freeSpace)(devices)

QN_FUSION_DECLARE_FUNCTIONS(AddStorageResponse, (json), NX_NETWORK_API)

} // namespace nx::cloud::storage::service::api