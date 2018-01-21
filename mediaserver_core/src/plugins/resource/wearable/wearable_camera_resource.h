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

    virtual QnConstResourceAudioLayoutPtr getAudioLayout(
        const QnAbstractStreamDataProvider* dataProvider = 0) const override;

protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual CameraDiagnostics::Result initInternal() override;
};

using QnWearableCameraResourcePtr = QnSharedResourcePointer<QnWearableCameraResource>;

