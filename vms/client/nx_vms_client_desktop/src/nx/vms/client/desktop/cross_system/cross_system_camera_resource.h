// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/client_camera.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/api/data/camera_data_ex.h>
#include <nx/vms/common/resource/resource_descriptor.h>

#include "cloud_cross_system_context.h"

namespace nx::vms::client::desktop {

class CrossSystemCameraResource: public QnClientCameraResource
{
public:
    CrossSystemCameraResource(
        CloudCrossSystemContext* crossSystemContext,
        const nx::vms::api::CameraDataEx& source);
    CrossSystemCameraResource(
        CloudCrossSystemContext* crossSystemContext,
        const nx::vms::common::ResourceDescriptor& descriptor);
    ~CrossSystemCameraResource() override;

    void update(nx::vms::api::CameraDataEx data);

    api::ResourceStatus getStatus() const override;
    CloudCrossSystemContext* crossSystemContext() const;
    nx::vms::common::ResourceDescriptor descriptor() const;

private:
    void watchOnCrossSystemContext();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

using CrossSystemCameraResourcePtr = QnSharedResourcePointer<CrossSystemCameraResource>;
using CrossSystemCameraResourceList = QnSharedResourcePointerList<CrossSystemCameraResource>;

} // namespace nx::vms::client::desktop
