// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_camera_preloader_resource.h"

#include <nx/vms/client/core/system_context.h>
#include <nx/vms/common/system_context.h>
#include <utils/common/id.h>

namespace nx::vms::client::desktop {

namespace {

const nx::Uuid kDesktopCameraPreloaderResourceTypeId("{82d232d7-0c67-4d8e-b5a8-a0d5075ff3a5}");

} // namespace

DesktopCameraPreloaderResource::DesktopCameraPreloaderResource(
    const nx::Uuid& id,
    const QString& physicalId)
    :
    QnClientCameraResource(kDesktopCameraPreloaderResourceTypeId)
{
    setIdUnsafe(id);
    setPhysicalId(physicalId);
    addFlags(Qn::fake | Qn::desktop_camera);
}

} // namespace nx::vms::client::desktop
