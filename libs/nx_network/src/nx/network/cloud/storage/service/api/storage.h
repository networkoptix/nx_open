#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx::cloud::storage::service::api {

struct NX_NETWORK_API Device
{
    std::string type;
    std::string dataUrl;
};

#define Device_Fields (type)(dataUrl)

QN_FUSION_DECLARE_FUNCTIONS(Device, (json), NX_NETWORK_API)

//-------------------------------------------------------------------------------------------------
// Storage

struct NX_NETWORK_API Storage
{
    std::string id;
    int totalSpace = 0;
    int freeSpace = 0;
    std::string region;
    Device ioDevice;
};

#define Storage_Fields (id)(totalSpace)(freeSpace)(region)(ioDevice)

QN_FUSION_DECLARE_FUNCTIONS(Storage, (json), NX_NETWORK_API)

} // namespace nx::cloud::storage::service::api