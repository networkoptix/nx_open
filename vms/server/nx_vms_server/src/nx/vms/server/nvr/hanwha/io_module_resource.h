#pragma once

#include <nx/vms/server/resource/camera.h>

namespace nx::vms::server::nvr::hanwha {

class IoModuleResource: public nx::vms::server::resource::Camera
{
public:
    IoModuleResource(QnMediaServerModule* serverModule);

    virtual QString getDriverName() const override;

    virtual QnIOStateDataList ioPortStates() const override;

    virtual bool setOutputPortState(
        const QString& outputId, bool isActive, unsigned int autoResetTimeoutMs) override;

protected:
    virtual CameraDiagnostics::Result initializeCameraDriver() override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override { return nullptr; }

    virtual void startInputPortStatesMonitoring() override;
    virtual void stopInputPortStatesMonitoring() override;
};

} // namespace nx::vms::server::nvr::hanwha
