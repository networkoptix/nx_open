#pragma once

#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/i_plugin_event.h>

#include <core/resource/resource_fwd.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/server_module_aware.h>

#include <nx/vms/server/analytics/metadata_handler.h>

namespace nx::vms::server::analytics {

class DeviceAgentHandler:
    public QObject,
    public nx::vms::server::ServerModuleAware,
    public nx::sdk::analytics::IDeviceAgent::IHandler
{
    Q_OBJECT

public:
    DeviceAgentHandler(
        QnMediaServerModule* serverModule,
        resource::AnalyticsEngineResourcePtr engineResource,
        QnVirtualCameraResourcePtr device);

    virtual void handleMetadata(nx::sdk::analytics::IMetadataPacket* metadataPacket) override;
    virtual void handlePluginEvent(nx::sdk::IPluginEvent* sdkPluginEvent) override;

    void setMetadataSink(QnAbstractDataReceptor* metadataSink);

signals:
    void pluginEventTriggered(const nx::vms::event::PluginEventPtr& pluginEvent);

private:
    resource::AnalyticsEngineResourcePtr m_engineResource;
    QnVirtualCameraResourcePtr m_device;
    nx::vms::server::analytics::MetadataHandler m_metadataHandler;
};

} // namespace nx::vms::server::analytics
