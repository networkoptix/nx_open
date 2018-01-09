#pragma once

#include "core/resource/camera_resource.h"


class QnWearableCameraResource : public QnPhysicalCameraResource {
    Q_OBJECT

public:
    static const QString MANUFACTURE;

    QnWearableCameraResource();
    virtual ~QnWearableCameraResource();

    virtual QString getDriverName() const override;

protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
};

typedef QnSharedResourcePointer<QnWearableCameraResource> QnWearableCameraResourcePtr;

