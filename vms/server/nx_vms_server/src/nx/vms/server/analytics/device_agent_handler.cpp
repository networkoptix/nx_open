#include "device_agent_handler.h"

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_resource.h>

#include <nx/vms/event/events/plugin_diagnostic_event.h>

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
    connect(this, &DeviceAgentHandler::pluginDiagnosticEventTriggered,
        serverModule->eventConnector(), &event::EventConnector::at_pluginDiagnosticEvent,
        Qt::QueuedConnection);

    m_metadataHandler.setEventTypeDescriptors(serverModule->commonModule()
        ->analyticsEventTypeDescriptorManager()->supportedEventTypeDescriptors(m_device));
}

void DeviceAgentHandler::handleMetadata(nx::sdk::analytics::IMetadataPacket* metadataPacket)
{
    m_metadataHandler.handleMetadata(metadataPacket);
}

void DeviceAgentHandler::handlePluginDiagnosticEvent(
    nx::sdk::IPluginDiagnosticEvent* sdkPluginDiagnosticEvent)
{
    nx::vms::event::PluginDiagnosticEventPtr pluginDiagnosticEvent(
        new nx::vms::event::PluginDiagnosticEvent(
            qnSyncTime->currentUSecsSinceEpoch(),
            m_engineResourceId,
            sdkPluginDiagnosticEvent->caption(),
            sdkPluginDiagnosticEvent->description(),
            nx::vms::server::sdk_support::fromPluginDiagnosticEventLevel(
                sdkPluginDiagnosticEvent->level()),
            m_device));

    emit pluginDiagnosticEventTriggered(pluginDiagnosticEvent);
}

void DeviceAgentHandler::setMetadataSink(QnAbstractDataReceptor* metadataSink)
{
    m_metadataHandler.setMetadataSink(metadataSink);
}

} // namespace nx::vms::server::analytics
