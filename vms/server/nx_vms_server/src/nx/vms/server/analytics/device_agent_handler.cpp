#include "device_agent_handler.h"

#include <core/resource/resource_fwd.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>

#include <nx/vms/event/events/plugin_event.h>

#include <nx/vms/server/sdk_support/utils.h>

#include <nx/vms/server/event/event_connector.h>

#include <nx/analytics/descriptor_list_manager.h>

#include <utils/common/synctime.h>

namespace nx::vms::server::analytics {

DeviceAgentHandler::DeviceAgentHandler(
    QnMediaServerModule* serverModule,
    resource::AnalyticsEngineResourcePtr engineResource,
    QnVirtualCameraResourcePtr device)
    :
    ServerModuleAware(serverModule),
    m_engineResource(std::move(engineResource)),
    m_device(std::move(device)),
    m_metadataHandler(serverModule)
{
    connect(this, &DeviceAgentHandler::pluginEventTriggered,
        serverModule->eventConnector(), &event::EventConnector::at_pluginEvent,
        Qt::QueuedConnection);

    m_metadataHandler.setResource(m_device);
    m_metadataHandler.setPluginId(m_engineResource->plugin()->manifest().id);

    const auto descriptorListManager = serverModule
        ->commonModule()
        ->analyticsDescriptorListManager();

    auto eventDescriptors = descriptorListManager
        ->deviceDescriptors<nx::vms::api::analytics::EventTypeDescriptor>(m_device);

    m_metadataHandler.setEventTypeDescriptors(eventDescriptors);
}

void DeviceAgentHandler::handleMetadata(nx::sdk::analytics::IMetadataPacket* metadataPacket)
{
    m_metadataHandler.handleMetadata(metadataPacket);
}

void DeviceAgentHandler::handlePluginEvent(nx::sdk::IPluginEvent* sdkPluginEvent)
{
    nx::vms::event::PluginEventPtr pluginEvent(
        new nx::vms::event::PluginEvent(
            qnSyncTime->currentUSecsSinceEpoch(),
            m_engineResource->getId(),
            sdkPluginEvent->caption(),
            sdkPluginEvent->description(),
            nx::vms::server::sdk_support::fromSdkPluginEventLevel(sdkPluginEvent->level()),
            {m_device->getId().toString()}));

    emit pluginEventTriggered(pluginEvent);
}

void DeviceAgentHandler::setMetadataSink(QnAbstractDataReceptor* metadataSink)
{
    m_metadataHandler.setMetadataSink(metadataSink);
}

} // namespace nx::vms::server::analytics
