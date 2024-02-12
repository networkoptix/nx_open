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
    Q_OBJECT

public:
    /** Generic constructor called when full camera data is available. */
    CrossSystemCameraResource(
        const QString& systemId,
        const nx::vms::api::CameraDataEx& source);

    /** Resource thumb constructor, used to create temporary camera replacement. */
    CrossSystemCameraResource(
        const QString& systemId,
        const nx::Uuid& id,
        const QString& name);
    ~CrossSystemCameraResource() override;

    void update(nx::vms::api::CameraDataEx data);

    /** Id of the system Camera belongs to. */
    QString systemId() const;

    const std::optional<nx::vms::api::CameraDataEx>& source() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

using CrossSystemCameraResourcePtr = QnSharedResourcePointer<CrossSystemCameraResource>;
using CrossSystemCameraResourceList = QnSharedResourcePointerList<CrossSystemCameraResource>;

} // namespace nx::vms::client::desktop
