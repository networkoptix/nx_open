#pragma once

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/i_plugin_diagnostic_event.h>

#include <core/resource/resource_fwd.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/server_module_aware.h>

#include <nx/vms/server/analytics/types.h>
#include <nx/vms/server/analytics/wrappers/types.h>
#include <nx/vms/server/analytics/metadata_handler.h>

namespace nx::vms::server::analytics {
    class DeviceAgentHandler:
    public QObject,
    public /*mixin*/ nx::vms::server::ServerModuleAware,
    public nx::sdk::RefCountable<nx::sdk::analytics::IDeviceAgent::IHandler>
{
    Q_OBJECT

    using DeviceAgentManifest = nx::vms::api::analytics::DeviceAgentManifest;
    using DeviceAgentManifestHandler = std::function<void(const DeviceAgentManifest&)>;

public:
    DeviceAgentHandler(
        QnMediaServerModule* serverModule,
        resource::AnalyticsEngineResourcePtr engine,
        resource::CameraPtr device,
        DeviceAgentManifestHandler manifestHandler);

    virtual ~DeviceAgentHandler();

    virtual void handleMetadata(nx::sdk::analytics::IMetadataPacket* metadataPacket) override;
    virtual void handlePluginDiagnosticEvent(
        nx::sdk::IPluginDiagnosticEvent* sdkPluginDiagnosticEvent) override;

    virtual void pushManifest(const nx::sdk::IString* manifest) override;

    void setMetadataSinks(MetadataSinkSet metadataSinks);

signals:
    void pluginDiagnosticEventTriggered(
        const nx::vms::event::PluginDiagnosticEventPtr& pluginDiagnosticEvent);

private:
    void handleMetadataViolation(const wrappers::Violation& violation);

private:
    resource::AnalyticsEngineResourcePtr m_engine;
    resource::CameraPtr m_device;
    nx::vms::server::analytics::MetadataHandler m_metadataHandler;
    DeviceAgentManifestHandler m_manifestHandler;
};

} // namespace nx::vms::server::analytics
