// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

namespace nx::vms::client::core {

void ServerResource::setStatus(
    nx::vms::api::ResourceStatus newStatus,
    Qn::StatusChangeReason reason)
{
    base_type::setStatus(newStatus, reason);
    if (auto resourcePool = this->resourcePool())
    {
        auto childCameras = resourcePool->getAllCameras(getId());
        for (const auto& camera: childCameras)
        {
            NX_VERBOSE(this, "%1 Emit statusChanged signal for resource %2", __func__, camera);
            emit camera->statusChanged(camera, Qn::StatusChangeReason::Local);
        }
    }
}

} // namespace nx::vms::client::core
