// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/camera_resource.h>

#include "resource_fwd.h"

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API Camera: public QnVirtualCameraResource
{
    Q_OBJECT
    using base_type = QnVirtualCameraResource;

public:
    explicit Camera(const nx::Uuid& resourceTypeId);

    /**
     * @return User-defined camera name if it is present, default name otherwise.
     */
    virtual QString getName() const override;

    /**
     * Set user-defined camera name.
     */
    virtual void setName(const QString& name) override;

    virtual nx::vms::api::ResourceStatus getStatus() const override;

    virtual Qn::ResourceFlags flags() const override;
    virtual void setParentId(const nx::Uuid& parent) override;

    bool isPtzSupported() const;
    bool isPtzRedirected() const;
    CameraPtr ptzRedirectedTo() const;

protected:
    virtual void updateInternal(const QnResourcePtr& source, NotifierList& notifiers) override;
};

} // namespace nx::vms::client::core
