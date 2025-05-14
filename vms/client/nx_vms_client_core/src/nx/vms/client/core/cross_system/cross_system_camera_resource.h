// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/api/data/camera_data_ex.h>
#include <nx/vms/client/core/resource/camera_resource.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API CrossSystemCameraResource: public CameraResource
{
    Q_OBJECT
    using base_type = CameraResource;

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

    virtual ~CrossSystemCameraResource() override;

    void update(nx::vms::api::CameraDataEx data);

    /** Id of the system Camera belongs to. */
    QString systemId() const;

    const std::optional<nx::vms::api::CameraDataEx>& source() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
