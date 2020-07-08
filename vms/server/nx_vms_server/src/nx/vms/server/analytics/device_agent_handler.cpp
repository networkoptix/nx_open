#include "device_agent_handler.h"

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_resource.h>

#include <nx/vms/event/events/plugin_diagnostic_event.h>

#include <nx/vms/server/sdk_support/utils.h>

#include <nx/vms/server/event/event_connector.h>

#include <nx/analytics/descriptor_manager.h>

#include <utils/common/synctime.h>

#include <nx/vms/server/analytics/wrappers/manifest_converter.h>
#include <nx/vms/server/analytics/wrappers/string_builder.h>

namespace nx::vms::server::analytics {

DeviceAgentHandler::DeviceAgentHandler(
    QnMediaServerModule* serverModule,
    resource::AnalyticsEngineResourcePtr engine,
    resource::CameraPtr device,
    DeviceAgentManifestHandler manifestHandler)
    :
    ServerModuleAware(serverModule),
    m_engine(std::move(engine)),
    m_device(std::move(device)),
    m_metadataHandler(serverModule, m_device, m_engine->getId()),
    m_manifestHandler(std::move(manifestHandler))
{
    connect(this, &DeviceAgentHandler::pluginDiagnosticEventTriggered,
        serverModule->eventConnector(), &event::EventConnector::at_pluginDiagnosticEvent,
        Qt::QueuedConnection);
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
            m_engine->getId(),
            sdkPluginDiagnosticEvent->caption(),
            sdkPluginDiagnosticEvent->description(),
            nx::vms::server::sdk_support::fromPluginDiagnosticEventLevel(
                sdkPluginDiagnosticEvent->level()),
            m_device));

    emit pluginDiagnosticEventTriggered(pluginDiagnosticEvent);
}

void DeviceAgentHandler::setMetadataSinks(MetadataSinkSet metadataSinks)
{
    m_metadataHandler.setMetadataSinks(std::move(metadataSinks));
}

void DeviceAgentHandler::pushManifest(const nx::sdk::IString* manifestString)
{
    const wrappers::SdkObjectDescription sdkObjectDescription(
        m_engine->plugin().dynamicCast<resource::AnalyticsPluginResource>(), m_engine, m_device);

    wrappers::ManifestConverter<DeviceAgentManifest> manifestConverter(
        sdkObjectDescription,
        [this, &sdkObjectDescription](wrappers::Violation violation)
        {
            const wrappers::StringBuilder stringBuilder(
                wrappers::SdkMethod::pushManifest,
                sdkObjectDescription,
                violation);

            NX_DEBUG(this, stringBuilder.buildLogString());

            nx::vms::event::PluginDiagnosticEventPtr pluginDiagnosticEvent(
                new nx::vms::event::PluginDiagnosticEvent(
                    qnSyncTime->currentUSecsSinceEpoch(),
                    m_engine->getId(),
                    stringBuilder.buildPluginDiagnosticEventCaption(),
                    stringBuilder.buildPluginDiagnosticEventDescription(),
                    nx::vms::api::EventLevel::ErrorEventLevel,
                    m_device));

            emit pluginDiagnosticEventTriggered(pluginDiagnosticEvent);
        });

    const sdk::Ptr<const sdk::IString> manifestStringPtr =
        manifestString->queryInterface<nx::sdk::IString>();

    const std::optional<DeviceAgentManifest> manifest =
        manifestConverter.convert(manifestStringPtr);

    if (manifest && m_manifestHandler)
        m_manifestHandler(*manifest);
}

} // namespace nx::vms::server::analytics
