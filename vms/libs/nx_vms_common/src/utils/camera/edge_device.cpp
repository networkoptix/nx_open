// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "edge_device.h"

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

namespace nx::vms::common {
namespace utils {
namespace edge_device {

bool isCoupledEdgeCamera(
    const QnMediaServerResourcePtr& edgeServer,
    const QnVirtualCameraResourcePtr& camera)
{
    using namespace nx::network;
    using namespace nx::vms::api;

    if (!NX_ASSERT(edgeServer && camera, "Unexpected null parameter"))
        return false;

    if (!NX_ASSERT(edgeServer->getServerFlags().testFlag(SF_Edge), "EDGE server expected"))
        return false;

    if (camera->hasFlags(Qn::virtual_camera)
        || camera->hasFlags(Qn::desktop_camera)
        || camera->hasFlags(Qn::io_module)
        || camera->isDtsBased())
    {
        return false;
    }

    const auto cameraHostAddressString = camera->getHostAddress();
    if (cameraHostAddressString.isEmpty())
        return false;

    const auto cameraHostAddress = HostAddress(cameraHostAddressString);
    const auto serverAddressList = edgeServer->getNetAddrList();

    return std::any_of(std::cbegin(serverAddressList), std::cend(serverAddressList),
        [&cameraHostAddress](const auto& serverAddress)
        {
            return serverAddress.address == cameraHostAddress;
        });
}

} // namespace edge_device
} // namespace utils
} // namespace nx::vms::common
