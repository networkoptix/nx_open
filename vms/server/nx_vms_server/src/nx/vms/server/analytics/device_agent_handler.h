#pragma once

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/i_plugin_diagnostic_event.h>

#include <core/resource/resource_fwd.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/server_module_aware.h>

#include <nx/vms/server/analytics/metadata_handler.h>

namespace nx::vms::server::analytics {

class DeviceAgentHandler:
    public QObject,
    public /*mixin*/ nx::vms::server::ServerModuleAware,
    public nx::sdk::RefCountable<nx::sdk::analytics::IDeviceAgent::IHandler>
{
    Q_OBJECT

public:
    DeviceAgentHandler(
        QnMediaServerModule* serverModule,
        QnUuid engineResourceId,
        QnVirtualCameraResourcePtr device);

    virtual void handleMetadata(nx::sdk::analytics::IMetadataPacket* metadataPacket) override;
    virtual void handlePluginDiagnosticEvent(
        nx::sdk::IPluginDiagnosticEvent* sdkPluginDiagnosticEvent) override;

    void setMetadataSink(QnAbstractDataReceptor* metadataSink);

signals:
    void pluginDiagnosticEventTriggered(
        const nx::vms::event::PluginDiagnosticEventPtr& pluginDiagnosticEvent);

private:
    QnUuid m_engineResourceId;
    QnVirtualCameraResourcePtr m_device;
    nx::vms::server::analytics::MetadataHandler m_metadataHandler;
};

} // namespace nx::vms::server::analytics
