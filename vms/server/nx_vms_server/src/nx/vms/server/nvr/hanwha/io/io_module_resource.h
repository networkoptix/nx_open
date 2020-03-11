#pragma once

#include <optional>

#include <nx/vms/server/resource/camera.h>
#include <nx/vms/server/nvr/types.h>

namespace nx::vms::server::nvr::hanwha {

class IoModuleResource: public nx::vms::server::resource::Camera
{
    struct ExtendedIoPortState: public QnIOStateData
    {
        bool isActiveAsOpenCircuit = false;
    };

    using base_type = nx::vms::server::resource::Camera;
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
    void updatePortDescriptions(QnIOPortDataList portDescriptors);

    std::optional<QnIOPortData> portDescriptor(
        const QString& portId,
        Qn::IOPortType portType = Qn::IOPortType::PT_Unknown) const;

    void handleStateChange(const QnIOStateDataList& state);

    ExtendedIoPortState currentPortState(const QString& portId) const;

    void updatePortState(ExtendedIoPortState newPortStateAsOpenCircuit);

    void emitSignals(const QnIOPortData& portDescriptor, const QnIOStateData& portState);

    // Since the NVR IO manager always provides port state as if it works in the open cicuit mode,
    // it needs to be translated. When reading port IO states they have to be translated
    // from the open circuit to the current circuit "coordinate system". Reverse translation has to
    // be performed when changing an output port state. This method is able to translate port states
    // in both directions.
    bool isActiveTranslated(const QnIOPortData& portDescription, bool isActive) const;

    QnIOPortDataList mergedPortDescriptions();

private:
    void at_propertyChanged(const QnResourcePtr& resource, const QString& key);

private:
    mutable QnMutex m_mutex;
    HandlerId m_handlerId = 0;

    std::map<QString, QnIOPortData> m_portDescriptorsById;
    std::map<QString, ExtendedIoPortState> m_portStateById;
};

} // namespace nx::vms::server::nvr::hanwha
