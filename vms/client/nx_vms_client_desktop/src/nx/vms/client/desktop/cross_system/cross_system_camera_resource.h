// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/client_camera.h>
#include <nx/vms/api/data/camera_data_ex.h>

namespace nx::vms::client::desktop {

class CrossSystemCameraResource: public QnClientCameraResource
{
public:
    CrossSystemCameraResource(nx::vms::api::CameraDataEx source);

    void update(nx::vms::api::CameraDataEx data);

private:
    nx::vms::api::CameraDataEx m_source;
};

using CrossSystemCameraResourcePtr = QnSharedResourcePointer<CrossSystemCameraResource>;
using CrossSystemCameraResourceList = QnSharedResourcePointerList<CrossSystemCameraResource>;

} // namespace nx::vms::client::desktop
