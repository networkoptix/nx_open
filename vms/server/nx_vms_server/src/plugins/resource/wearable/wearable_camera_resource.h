#pragma once

#include <nx/vms/server/resource/camera.h>

class QnWearableCameraResource: public nx::vms::server::resource::Camera
{
    Q_OBJECT
    using base_type = nx::vms::server::resource::Camera;

public:
    static const QString kManufacture;

    QnWearableCameraResource(QnMediaServerModule* serverModule);
    virtual ~QnWearableCameraResource() override;

    virtual QString getDriverName() const override;

    virtual void setStatus(Qn::ResourceStatus newStatus, Qn::StatusChangeReason reason) override;

    virtual QnConstResourceAudioLayoutPtr getAudioLayout(
        const QnAbstractStreamDataProvider* dataProvider = 0) const override;

    virtual nx::vms::server::resource::StreamCapabilityMap getStreamCapabilityMapFromDriver(
        Qn::StreamIndex streamIndex) override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;
protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual CameraDiagnostics::Result initInternal() override;
};

using QnWearableCameraResourcePtr = QnSharedResourcePointer<QnWearableCameraResource>;

