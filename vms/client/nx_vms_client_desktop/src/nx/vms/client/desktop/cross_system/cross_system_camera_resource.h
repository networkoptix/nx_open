// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/client_camera.h>
#include <nx/vms/api/data/device_model.h>

namespace nx::vms::client::desktop {

class CrossSystemCameraResource: public QnClientCameraResource
{
public:
    CrossSystemCameraResource(nx::vms::api::DeviceModel source);

    void update(nx::vms::api::DeviceModel data);

private:
    nx::vms::api::DeviceModel m_source;
};

using CrossSystemCameraResourcePtr = QnSharedResourcePointer<CrossSystemCameraResource>;
using CrossSystemCameraResourceList = QnSharedResourcePointerList<CrossSystemCameraResource>;

} // namespace nx::vms::client::desktop
