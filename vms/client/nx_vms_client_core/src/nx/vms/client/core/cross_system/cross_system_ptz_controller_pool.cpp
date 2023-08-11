// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cross_system_ptz_controller_pool.h"

#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/ptz/caching_ptz_controller.h>
#include <nx/vms/client/core/ptz/remote_ptz_controller.h>
#include <nx/vms/client/core/resource/camera.h>
#include <utils/common/delete_later.h>

namespace nx::vms::client::core {

CrossSystemPtzControllerPool::CrossSystemPtzControllerPool(
    nx::vms::common::SystemContext* systemContext,
    QObject* parent)
    :
    base_type(systemContext, parent)
{
    // Delay controller initialization for the cross-system cameras until they are actually placed
    // on the scene.
    connect(resourcePool(),
        &QnResourcePool::resourceRemoved,
        this,
        &CrossSystemPtzControllerPool::unregisterResource);
}

void CrossSystemPtzControllerPool::createControllerIfNeeded(const QnResourcePtr& resource)
{
    if (!controller(resource))
        registerResource(resource);
}

QnPtzControllerPtr CrossSystemPtzControllerPool::createController(
    const QnResourcePtr& resource) const
{
    const auto camera = resource.dynamicCast<nx::vms::client::core::Camera>();
    if (!camera || !camera->isPtzSupported())
        return {};

    QnPtzControllerPtr baseController(new ptz::RemotePtzController(camera));
    QnPtzControllerPtr result(new ptz::CachingPtzController(baseController), &qnDeleteLater);
    return result;
}

} // namespace nx::vms::client::core
