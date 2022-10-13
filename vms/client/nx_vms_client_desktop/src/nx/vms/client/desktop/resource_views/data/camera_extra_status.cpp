// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_extra_status.h"

#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <nx/vms/common/system_context.h>

namespace nx::vms::client::desktop {

CameraExtraStatus getCameraExtraStatus(const QnVirtualCameraResourcePtr& camera)
{
    using namespace nx::vms::api;

    CameraExtraStatus result;
    if (!camera || camera->hasFlags(Qn::cross_system))
        return result;

    if (camera->getStatus() == ResourceStatus::recording)
        result.setFlag(CameraExtraStatusFlag::recording);

    if (camera->isScheduleEnabled())
        result.setFlag(CameraExtraStatusFlag::scheduled);

    if (camera->statusFlags().testFlag(CameraStatusFlag::CSF_HasIssuesFlag)
        || camera->statusFlags().testFlag(CameraStatusFlag::CSF_InvalidScheduleFlag))
    {
        result.setFlag(CameraExtraStatusFlag::buggy);
    }

    if (const auto context = camera->systemContext())
    {
        if (!context->cameraHistoryPool()->getCameraFootageData(camera->getId()).empty())
            result.setFlag(CameraExtraStatusFlag::hasArchive);
    }

    return result;
}

} // namespace nx::vms::client::desktop
