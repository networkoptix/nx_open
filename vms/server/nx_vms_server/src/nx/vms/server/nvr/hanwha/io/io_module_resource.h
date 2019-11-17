#pragma once

#include <optional>

#include <nx/vms/server/resource/camera.h>

namespace nx::vms::server::nvr::hanwha {

class IoModuleResource: public nx::vms::server::resource::Camera
{
    using base_type = nx::vms::server::resource::Camera;
public:
    IoModuleResource(QnMediaServerModule* serverModule);

    virtual QString getDriverName() const override;

    virtual QnIOStateDataList ioPortStates() const override;

    virtual bool setOutputPortState(
        const QString& outputId, bool isActive, unsigned int autoResetTimeoutMs) override;

    virtual bool setIoPortDescriptions(QnIOPortDataList ports, bool needMerge) override;

protected:
    virtual CameraDiagnostics::Result initializeCameraDriver() override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override { return nullptr; }

    virtual void startInputPortStatesMonitoring() override;
    virtual void stopInputPortStatesMonitoring() override;

private:
    std::optional<QnIOPortData> portDescriptor(
        const QString& portId,
        Qn::IOPortType portType = Qn::IOPortType::PT_Unknown) const;

    void handleStateChange(const QnIOStateDataList& state);

private:
    mutable QnMutex m_mutex;
    QnUuid m_handlerId;
    std::map<QString, QnIOPortData> m_portDescriptorsById;
};

} // namespace nx::vms::server::nvr::hanwha
