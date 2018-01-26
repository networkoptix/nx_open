#pragma once

#include "core/resource/camera_resource.h"

class QnWearableCameraResource: public QnPhysicalCameraResource
{
    Q_OBJECT
    using base_type = QnPhysicalCameraResource;

public:
    static const QString kManufacture;

    QnWearableCameraResource();
    virtual ~QnWearableCameraResource() override;

    virtual QString getDriverName() const override;

    virtual void setStatus(Qn::ResourceStatus newStatus, Qn::StatusChangeReason reason) override;

protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
};

using QnWearableCameraResourcePtr = QnSharedResourcePointer<QnWearableCameraResource>;

