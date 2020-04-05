#pragma once

#include <optional>

#include <nx/vms/server/resource/camera.h>
#include <nx/vms/server/nvr/types.h>

namespace nx::vms::server::nvr::hanwha {

class IoModuleResource: public nx::vms::server::resource::Camera
{
    using base_type = nx::vms::server::resource::Camera;

    struct PortStateNotification
    {
        QnIOStateData portState;
        Qn::IOPortType portType;
    };

public:
    IoModuleResource(QnMediaServerModule* serverModule);

    virtual QString getDriverName() const override;

    virtual bool setOutputPortState(
        const QString& outputId, bool isActive, unsigned int autoResetTimeoutMs) override;

protected:
    virtual CameraDiagnostics::Result initializeCameraDriver() override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override { return nullptr; }

    virtual void startInputPortStatesMonitoring() override;
    virtual void stopInputPortStatesMonitoring() override;

private:
    void updatePortDescriptionsThreadUnsafe(QnIOPortDataList portDescriptions);

    std::optional<QnIOPortData> portDescriptionThreadUnsafe(const QString& portId) const;

    void emitSignals(const QnIOStateData& portState, Qn::IOPortType portType);

    QnIOPortDataList mergedPortDescriptions();

    void handleStateChange(const QnIOStateDataList& portStates);

private:
    void at_propertyChanged(const QnResourcePtr& resource, const QString& key);

private:
    mutable QnMutex m_mutex;
    HandlerId m_handlerId = 0;
    bool m_initialilStateHasBeenProcessed = false;
    std::map<QString, QnIOPortData> m_portDescriptions;
};

} // namespace nx::vms::server::nvr::hanwha
