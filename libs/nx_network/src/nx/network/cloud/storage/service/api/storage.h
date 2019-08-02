#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/url.h>

namespace nx::cloud::storage::service::api {

struct NX_NETWORK_API Device
{
    std::string type;
    nx::utils::Url dataUrl;
    std::string region;

    bool operator==(const Device& other) const;
};

#define Device_Fields (type)(dataUrl)(region)

QN_FUSION_DECLARE_FUNCTIONS(Device, (json), NX_NETWORK_API)

//-------------------------------------------------------------------------------------------------
// Storage

struct NX_NETWORK_API Storage
{
    std::string id;
    int totalSpace = 0;
    int freeSpace = 0;
    std::vector<Device> ioDevices;

    bool operator==(const Storage& other) const;
};

#define Storage_Fields (id)(totalSpace)(freeSpace)(ioDevices)

QN_FUSION_DECLARE_FUNCTIONS(Storage, (ubjson)(json), NX_NETWORK_API)

} // namespace nx::cloud::storage::service::api