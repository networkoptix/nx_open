#include "device_agent_handler.h"

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_resource.h>

#include <nx/vms/event/events/plugin_event.h>

#include <nx/vms/server/sdk_support/utils.h>

#include <nx/vms/server/event/event_connector.h>

#include <nx/analytics/descriptor_manager.h>

#include <utils/common/synctime.h>

namespace nx::vms::server::analytics {

DeviceAgentHandler::DeviceAgentHandler(
    QnMediaServerModule* serverModule,
    QnUuid engineResourceId,
    QnVirtualCameraResourcePtr device)
    :
    ServerModuleAware(serverModule),
    m_engineResourceId(std::move(engineResourceId)),
    m_device(std::move(device)),
    m_metadataHandler(serverModule, m_device, m_engineResourceId)
{
    connect(this, &DeviceAgentHandler::pluginEventTriggered,
        serverModule->eventConnector(), &event::EventConnector::at_pluginEvent,
        Qt::QueuedConnection);

    nx::analytics::EventTypeDescriptorManager descriptorManager(serverModule->commonModule());
    m_metadataHandler.setEventTypeDescriptors(
        descriptorManager.supportedEventTypeDescriptors(m_device));
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
            m_engineResourceId,
            sdkPluginEvent->caption(),
            sdkPluginEvent->description(),
            nx::vms::server::sdk_support::fromSdkPluginEventLevel(sdkPluginEvent->level()),
            m_device));

    emit pluginEventTriggered(pluginEvent);
}

void DeviceAgentHandler::setMetadataSink(QnAbstractDataReceptor* metadataSink)
{
    m_metadataHandler.setMetadataSink(metadataSink);
}

} // namespace nx::vms::server::analytics
