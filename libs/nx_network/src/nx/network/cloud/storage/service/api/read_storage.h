#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx::cloud::storage::service::api {

struct NX_NETWORK_API Device
{
    std::string type;
    std::string awsRegion;
    std::string dataUrl;
};

#define Device_Fields (type)(awsRegion)(dataUrl)

QN_FUSION_DECLARE_FUNCTIONS(Device, (json), NX_NETWORK_API)

//-------------------------------------------------------------------------------------------------
// ReadStorageResponse

struct NX_NETWORK_API ReadStorageResponse
{
    std::string id;
    int totalSpace = 0;
    int freeSpace = 0;
    std::vector<std::string> devices;
    Device ioDevice;
};

#define ReadStorageResponse_Fields (id)(totalSpace)(freeSpace)(devices)(ioDevice)

QN_FUSION_DECLARE_FUNCTIONS(ReadStorageResponse, (json), NX_NETWORK_API)

} // namespace nx::cloud::storage::service::api