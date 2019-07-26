#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/url.h>

namespace nx::cloud::storage::service::api {

struct NX_NETWORK_API Device
{
    std::string type;
    nx::utils::Url dataUrl;
};

#define Device_Fields (type)(dataUrl)

QN_FUSION_DECLARE_FUNCTIONS(Device, (json), NX_NETWORK_API)

//-------------------------------------------------------------------------------------------------
// Storage

struct NX_NETWORK_API Storage
{
    std::string id;
    std::string region;
    int totalSpace = 0;
    int freeSpace = 0;
    Device ioDevice;
};

#define Storage_Fields (id)(region)(totalSpace)(freeSpace)(ioDevice)

QN_FUSION_DECLARE_FUNCTIONS(Storage, (json), NX_NETWORK_API)

} // namespace nx::cloud::storage::service::api