#pragma once

#include <core/resource/camera_resource.h>

class QnClientCoreCamera: public QnVirtualCameraResource {
    Q_OBJECT

    typedef QnVirtualCameraResource base_type;

public:
    QnClientCoreCamera(const QnUuid& resourceTypeId);

    virtual QString getDriverName() const override;
    virtual void setIframeDistance(int frames, int timems) override;

    //! Overrides \a QnResource::getName. Returns camera name (from \a QnCameraUserAttributes)
    virtual QString getName() const override;

    //! Overrides \a QnResource::setName. Writes target name to \a QnCameraUserAttributes instead of resource field.
    virtual void setName(const QString& name) override;

    virtual Qn::ResourceStatus getStatus() const override;

    virtual Qn::ResourceFlags flags() const override;
    virtual void setParentId(const QnUuid& parent) override;

protected:
    virtual void updateInternal(const QnResourcePtr &other, Qn::NotifierList& notifiers) override;
};
