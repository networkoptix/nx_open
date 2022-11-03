// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_extra_status.h"

#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <nx/vms/common/system_context.h>

namespace nx::vms::client::desktop {

ResourceExtraStatus getResourceExtraStatus(const QnResourcePtr& resource)
{
    using namespace nx::vms::api;

    if (const auto layout = resource.dynamicCast<QnLayoutResource>())
        return layout->locked() ? ResourceExtraStatusFlag::locked : ResourceExtraStatusFlag::empty;

    if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        ResourceExtraStatus resourceExtraStatus;
        if (!camera || camera->hasFlags(Qn::cross_system))
            return resourceExtraStatus;

        if (camera->getStatus() == ResourceStatus::recording)
            resourceExtraStatus.setFlag(ResourceExtraStatusFlag::recording);

        if (camera->isScheduleEnabled())
            resourceExtraStatus.setFlag(ResourceExtraStatusFlag::scheduled);

        if (camera->statusFlags().testFlag(CameraStatusFlag::CSF_HasIssuesFlag)
            || camera->statusFlags().testFlag(CameraStatusFlag::CSF_InvalidScheduleFlag))
        {
            resourceExtraStatus.setFlag(ResourceExtraStatusFlag::buggy);
        }

        if (const auto context = camera->systemContext())
        {
            if (!context->cameraHistoryPool()->getCameraFootageData(camera->getId()).empty())
                resourceExtraStatus.setFlag(ResourceExtraStatusFlag::hasArchive);
        }

        return resourceExtraStatus;
    }

    return {};
}

} // namespace nx::vms::client::desktop
