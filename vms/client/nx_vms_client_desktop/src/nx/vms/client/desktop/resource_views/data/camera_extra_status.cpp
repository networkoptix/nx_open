#include "camera_extra_status.h"

#include <core/resource/camera_resource.h>

namespace nx::vms::client::desktop {

CameraExtraStatus getCameraExtraStatus(QnResourcePtr resource)
{
    CameraExtraStatus result;
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return result;

    if (camera->getStatus() == Qn::Recording)
        result.setFlag(CameraExtraStatusFlag::recording);

    if (camera->isLicenseUsed())
        result.setFlag(CameraExtraStatusFlag::scheduled);

    if (camera->statusFlags().testFlag(Qn::CameraStatusFlag::CSF_HasIssuesFlag))
        result.setFlag(CameraExtraStatusFlag::buggy);

    return result;
}

} // namespace nx::vms::client::desktop
