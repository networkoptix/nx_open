#pragma once

#include <nx/sdk/analytics/device_agent.h>
#include <nx/sdk/i_plugin_event.h>

#include <core/resource/resource_fwd.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <nx/mediaserver/resource/analytics_engine_resource.h>
#include <nx/mediaserver/server_module_aware.h>

#include <nx/mediaserver/analytics/metadata_handler.h>

namespace nx::mediaserver::analytics {

class DeviceAgentHandler:
    public QObject,
    public nx::mediaserver::ServerModuleAware,
    public nx::sdk::analytics::DeviceAgent::IHandler
{
    Q_OBJECT

public:
    DeviceAgentHandler(
        QnMediaServerModule* serverModule,
        resource::AnalyticsEngineResourcePtr engineResource,
        QnVirtualCameraResourcePtr device);

    virtual void handleMetadata(nx::sdk::analytics::MetadataPacket* metadataPacket) override;
    virtual void handlePluginEvent(nx::sdk::IPluginEvent* sdkPluginEvent) override;

    void setMetadataSink(QnAbstractDataReceptor* metadataSink);

signals:
    void pluginEventTriggered(const nx::vms::event::PluginEventPtr& pluginEvent);

private:
    resource::AnalyticsEngineResourcePtr m_engineResource;
    QnVirtualCameraResourcePtr m_device;
    nx::mediaserver::analytics::MetadataHandler m_metadataHandler;
};

} // namespace nx::mediaserver::analytics
