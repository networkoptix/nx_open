// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_replacement.h"

#include <core/resource/camera_resource.h>
#include <core/resource/resource_type.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>

namespace {

// Constant redefinition. The same constant defined in the nx_vms_server module is unreachable.
static constexpr auto kArchiveCameraResourceTypeName = "ARCHIVE_CAMERA";

bool isArchiveCamera(const QnVirtualCameraResourcePtr& camera)
{
    const auto resourceTypeName = qnResTypePool->getResourceType(camera->getTypeId())->getName();
    return resourceTypeName.compare(kArchiveCameraResourceTypeName, Qt::CaseSensitive) == 0;
}

} // namespace

namespace nx::vms::common {
namespace utils {
namespace camera_replacement {

using namespace nx::vms::api;

bool isReplacementCamera(const QnResourcePtr& resource)
{
    if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
        return camera->dataAccessId() != camera->getPhysicalId();

    return false;
}

bool isReplacedCamera(const QnResourcePtr& resource)
{
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return false;

    const auto resourcePool = camera->resourcePool();
    if (!NX_ASSERT(resourcePool))
        return false;

    return resourcePool->getResource<QnVirtualCameraResource>(
        [camera](const QnVirtualCameraResourcePtr& otherCamera)
        {
            if (camera == otherCamera)
                return false;

            return otherCamera->dataAccessId() == camera->getPhysicalId();
        });
}

bool cameraSupportsReplacement(const QnResourcePtr& resource)
{
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return false;

    return !camera->flags().testFlag(Qn::removed)
        && !camera->flags().testFlag(Qn::virtual_camera)
        && !camera->flags().testFlag(Qn::desktop_camera)
        && !camera->isMultiSensorCamera()
        && !camera->isNvr()
        && !camera->isIOModule()
        && !isArchiveCamera(camera);
}

bool cameraCanBeReplaced(const QnResourcePtr& resource)
{
    if (!cameraSupportsReplacement(resource))
        return false;

    if (resource->getStatus() != ResourceStatus::offline)
        return false;

    const auto parentResource = resource->getParentResource();
    return parentResource && parentResource->getStatus() == ResourceStatus::online;
}

bool cameraCanBeUsedAsReplacement(const QnResourcePtr& resource)
{
    if (!cameraSupportsReplacement(resource))
        return false;

    return resource->isOnline();
}

} // namespace camera_replacement
} // namespace utils
} // namespace nx::vms::common
