#pragma once

#include <nx/mediaserver/resource/camera.h>

class QnWearableCameraResource: public nx::mediaserver::resource::Camera
{
    Q_OBJECT
    using base_type = nx::mediaserver::resource::Camera;

public:
    static const QString kManufacture;

    QnWearableCameraResource();
    virtual ~QnWearableCameraResource() override;

    virtual QString getDriverName() const override;

    virtual void setStatus(Qn::ResourceStatus newStatus, Qn::StatusChangeReason reason) override;

    virtual QnConstResourceAudioLayoutPtr getAudioLayout(
        const QnAbstractStreamDataProvider* dataProvider = 0) const override;

    virtual nx::mediaserver::resource::StreamCapabilityMap getStreamCapabilityMapFromDrives(
        Qn::StreamIndex streamIndex) override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;
protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual CameraDiagnostics::Result initInternal() override;
};

using QnWearableCameraResourcePtr = QnSharedResourcePointer<QnWearableCameraResource>;

