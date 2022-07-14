// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_replacement.h"

#include <core/resource/camera_resource.h>
#include <core/resource/resource_type.h>
#include <nx/utils/log/assert.h>

namespace {

// Constant redefinition. The same constant defined in the nx_vms_server module is unreachable.
static constexpr auto kArchiveCameraResourceTypeName = "ARCHIVE_CAMERA";

// Constant redefinition. The same constant defined in the nx_vms_server module is unreachable.
static constexpr auto kReplacedWithIdPropertyName = "replaceWithId";

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
        && camera->hasVideo()
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

bool cameraCanBeUsedAsReplacement(
    const QnResourcePtr& cameraToBeReplaced,
    const QnResourcePtr& replacementCamera)
{
    if (!NX_ASSERT(cameraToBeReplaced && replacementCamera))
        return false;

    if (cameraToBeReplaced == replacementCamera)
        return false;

    if (cameraToBeReplaced->getParentId() != replacementCamera->getParentId())
        return false;

    if (!cameraSupportsReplacement(replacementCamera))
        return false;

    if (isReplacedCamera(replacementCamera))
        return false;

    return replacementCamera->isOnline();
}

bool isReplacedCamera(const QnResourcePtr& resource)
{
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return false;

    return !camera->getProperty(kReplacedWithIdPropertyName).isEmpty();
}

} // namespace camera_replacement
} // namespace utils
} // namespace nx::vms::common
