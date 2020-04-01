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
    void updatePortDescriptionsThreadUnsafe(QnIOPortDataList portDescriptors);

    std::optional<QnIOPortData> portDescriptionThreadUnsafe(
        const QString& portId,
        Qn::IOPortType portType = Qn::IOPortType::PT_Unknown) const;

    /** @return List of ports states that have been changed. */
    std::vector<PortStateNotification> updatePortStatesThreadUnsafe(
        const QnIOStateDataList& state);

    QnIOStateData currentPortStateThreadUnsafe(const QString& portId) const;

    QnIOStateDataList currentPortStatesThreadUnsafe() const;

    void emitSignals(const QnIOStateData& portState, Qn::IOPortType portType);

    // Since the NVR IO manager always provides port state as if it works in the open cicuit mode,
    // it needs to be translated. When reading port IO states they have to be translated
    // from the open circuit to the current circuit "coordinate system". Reverse translation has to
    // be performed when changing an output port state. This method is able to translate port states
    // in both directions.
    bool isActiveTranslated(const QnIOPortData& portDescription, bool isActive) const;

    QnIOPortDataList mergedPortDescriptions();

    void handleStateChange(const QnIOStateDataList& portStates);

private:
    void at_propertyChanged(const QnResourcePtr& resource, const QString& key);

private:
    mutable QnMutex m_mutex;
    HandlerId m_handlerId = 0;

    std::map<QString, QnIOPortData> m_portDescriptorsById;
    std::map<QString, QnIOStateData> m_portStateById;
};

} // namespace nx::vms::server::nvr::hanwha
