#pragma once

#include "core/resource/camera_resource.h"

class QnWearableCameraResource: public QnPhysicalCameraResource
{
    Q_OBJECT

public:
    static const QString kManufacture;

    QnWearableCameraResource();
    virtual ~QnWearableCameraResource() override;

    virtual QString getDriverName() const override;

protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
};

using QnWearableCameraResourcePtr = QnSharedResourcePointer<QnWearableCameraResource>;

