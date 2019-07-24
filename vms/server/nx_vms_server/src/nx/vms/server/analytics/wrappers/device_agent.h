#pragma once

#include <optional>

#include <core/resource/resource_fwd.h>

#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/server/analytics/wrappers/types.h>
#include <nx/vms/server/sdk_support/error.h>

#include <nx/sdk/result.h>
#include <nx/sdk/i_string.h>
#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/i_consuming_device_agent.h>

#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <nx/vms/event/events/events_fwd.h>
#include <nx/vms/event/events/plugin_diagnostic_event.h>

namespace nx::vms::server::analytics::wrappers {

class DeviceAgent:
    public QObject,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
    Q_OBJECT;
public:
    DeviceAgent(QnMediaServerModule* serverModule);

    DeviceAgent(const DeviceAgent& other) = default;
    DeviceAgent(DeviceAgent&& other) = default;
    DeviceAgent& operator=(const DeviceAgent& other) = default;
    DeviceAgent& operator=(DeviceAgent&& other) = default;

    DeviceAgent(
        QnMediaServerModule* serverModule,
        sdk::Result<sdk::analytics::IDeviceAgent*> deviceAgentResult,
        resource::AnalyticsEngineResourcePtr engine,
        QnVirtualCameraResourcePtr device);

    sdk::Ptr<sdk::analytics::IEngine> engine() const;
    std::optional<ErrorMap> setSettings(const SettingMap& settings);
    std::optional<SettingsResponse> pluginSideSettings() const;
    std::optional<api::analytics::DeviceAgentManifest> manifest() const;
    void setHandler(sdk::Ptr<sdk::analytics::IDeviceAgent::IHandler> handler);
    bool setNeededMetadataTypes(const MetadataTypes& metadataTypes);
    bool pushDataPacket(sdk::Ptr<sdk::analytics::IDataPacket> data);

    bool isConsumer() const { return m_consumingDeviceAgent; }
    bool isValid() const { return m_deviceAgent; }
    operator bool() const { return m_deviceAgent; }

signals:
    void pluginDiagnosticEventTriggered(
        nx::vms::event::PluginDiagnosticEventPtr pluginDiagnosticEvent) const;

private:
    QString makeCaption(api::PredefinedPluginEventType violationType) const;
    QString makeDescription(api::PredefinedPluginEventType violationType) const;

    QString makeCaption(const sdk_support::Error& error) const;
    QString makeDescription(const sdk_support::Error& error) const;

    template<typename ReturnType>
    ReturnType handleContractViolation(
        api::PredefinedPluginEventType violationType,
        const QString& prefix = QString()) const;

    template<typename ReturnType>
    ReturnType handleError(
        const sdk_support::Error& error,
        const QString& prefix = QString()) const;

    void logError(const sdk_support::Error& error, const QString& prefix) const;
    void logContractViolation(
        api::PredefinedPluginEventType violationType,
        const QString& prefix) const;

    void throwPluginEvent(const QString& caption, const QString& description) const;

    sdk::Ptr<const sdk::IStringMap> prepareSettings(const SettingMap& settingsFromUser) const;

    sdk::Ptr<const sdk::IString> loadManifestFromFile() const;
    sdk::Ptr<const sdk::IString> loadManifestFromDeviceAgent() const;
    void dumpManifestToFileIfNeeded(const sdk::Ptr<const sdk::IString>) const;
    std::optional<api::analytics::DeviceAgentManifest> validateManifest(
        const sdk::Ptr<const sdk::IString> manfiestString) const;

private:
    resource::AnalyticsEngineResourcePtr m_engine;
    QnVirtualCameraResourcePtr m_device;
    sdk::Ptr<sdk::analytics::IDeviceAgent> m_deviceAgent;
    sdk::Ptr<sdk::analytics::IConsumingDeviceAgent> m_consumingDeviceAgent;
};

} // namespace nx::vms::server::analytics::wrappers
