#pragma once

#include <core/resource/camera_resource.h>

#include "client_core_resource_fwd.h"

namespace nx::vms::client::core {

class Camera: public QnVirtualCameraResource
{
    Q_OBJECT
    using base_type = QnVirtualCameraResource;

public:
    explicit Camera(const QnUuid& resourceTypeId);

    /**
     * @return User-defined camera name if it is present, default name otherwise.
     */
    virtual QString getName() const override;

    /**
     * Set user-defined camera name.
     */
    virtual void setName(const QString& name) override;

    virtual Qn::ResourceStatus getStatus() const override;

    virtual Qn::ResourceFlags flags() const override;
    virtual void setParentId(const QnUuid& parent) override;

    bool isPtzSupported() const;
    bool isPtzRedirected() const;
    CameraPtr ptzRedirectedTo() const;

protected:
    virtual void updateInternal(const QnResourcePtr& other, Qn::NotifierList& notifiers) override;
};

} // namespace nx::vms::client::core
