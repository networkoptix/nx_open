// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_replacement.h"

#include <core/resource/camera_resource.h>
#include <core/resource/resource_type.h>
#include <nx/utils/log/assert.h>

namespace nx::vms::common::utils::camera_replacement {

using namespace nx::vms::api;

namespace {

using namespace Qt::StringLiterals;

// Constant redefinition. The same constant defined in the nx_vms_server module is unreachable.
constexpr auto kArchiveCameraResourceTypeName = QStringView(u"ARCHIVE_CAMERA");

// Constant redefinition. The same constant defined in the nx_vms_server module is unreachable.
static const auto kReplacedWithIdPropertyName = u"replaceWithId"_s;

bool isArchiveCamera(const QnVirtualCameraResource& device)
{
    if (auto type = qnResTypePool->getResourceType(device.getTypeId()))
        return type->getName().compare(kArchiveCameraResourceTypeName, Qt::CaseSensitive) == 0;

    return false;
}

bool cameraSupportsReplacement(const QnVirtualCameraResource& device)
{
    return !device.flags().testFlag(Qn::removed)
           && !device.flags().testFlag(Qn::virtual_camera)
           && !device.flags().testFlag(Qn::desktop_camera)
           && !device.flags().testFlag(Qn::cross_system)
           && !device.isMultiSensorCamera()
           && !device.isNvr()
           && !device.isIOModule()
           && device.hasVideo()
           && !isArchiveCamera(device);
}

bool cameraCanBeReplaced(const QnVirtualCameraResource& device)
{
    if (!cameraSupportsReplacement(device))
        return false;

    if (device.getStatus() != ResourceStatus::offline)
        return false;

    const auto parentResource = device.getParentResource();
    return parentResource && parentResource->getStatus() == ResourceStatus::online;
}

} // namespace

bool cameraSupportsReplacement(const QnResourcePtr& resource)
{
    const auto device = resource.dynamicCast<QnVirtualCameraResource>();
    if (!device)
        return false;

    return cameraSupportsReplacement(*device);
}

bool cameraCanBeReplaced(const QnResourcePtr& resource)
{
    const auto device = resource.dynamicCast<QnVirtualCameraResource>();
    if (!device)
        return false;

    return cameraCanBeReplaced(*device);
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

} // namespace nx::vms::common::utils::camera_replacement
