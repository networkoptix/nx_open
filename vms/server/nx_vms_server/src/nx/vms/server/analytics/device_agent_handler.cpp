#include "device_agent_handler.h"

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_resource.h>

#include <plugins/vms_server_plugins_ini.h>

#include <nx/vms/event/events/plugin_diagnostic_event.h>

#include <nx/vms/server/sdk_support/utils.h>

#include <nx/vms/server/event/event_connector.h>

#include <nx/analytics/descriptor_manager.h>

#include <utils/common/synctime.h>

#include <nx/vms/server/analytics/wrappers/manifest_processor.h>
#include <nx/vms/server/analytics/wrappers/plugin_diagnostic_message_builder.h>

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
    m_metadataHandler(serverModule, m_device, m_engine->getId(),
        [this](const wrappers::Violation& violation) { handleMetadataViolation(violation); }),
    m_manifestHandler(std::move(manifestHandler))
{
    connect(this, &DeviceAgentHandler::pluginDiagnosticEventTriggered,
        serverModule->eventConnector(), &event::EventConnector::at_pluginDiagnosticEvent,
        Qt::QueuedConnection);
}

DeviceAgentHandler::~DeviceAgentHandler()
{
}

/** Called by MetadataHandler when the Plugin pushes bad data; produces PluginDiagnosticEvent. */
void DeviceAgentHandler::handleMetadataViolation(const wrappers::Violation& violation)
{
    const wrappers::PluginDiagnosticMessageBuilder messageBuilder(
        wrappers::SdkMethod::iHandler_handleMetadata,
        wrappers::SdkObjectDescription(
            m_engine->plugin().dynamicCast<resource::AnalyticsPluginResource>(),
            m_engine,
            m_device),
        violation);

    NX_DEBUG(this, messageBuilder.buildLogString());

    const nx::vms::event::PluginDiagnosticEventPtr pluginDiagnosticEvent(
        new nx::vms::event::PluginDiagnosticEvent(
            qnSyncTime->currentUSecsSinceEpoch(),
            m_engine->getId(),
            messageBuilder.buildPluginDiagnosticEventCaption(),
            messageBuilder.buildPluginDiagnosticEventDescription(),
            nx::vms::api::EventLevel::ErrorEventLevel,
            m_device));

    emit pluginDiagnosticEventTriggered(pluginDiagnosticEvent);
}

void DeviceAgentHandler::handleManifestError(
    const wrappers::PluginDiagnosticMessageBuilder& messageBuilder)
{
    NX_DEBUG(this, messageBuilder.buildLogString());

    const nx::vms::event::PluginDiagnosticEventPtr pluginDiagnosticEvent(
        new nx::vms::event::PluginDiagnosticEvent(
            qnSyncTime->currentUSecsSinceEpoch(),
            m_engine->getId(),
            messageBuilder.buildPluginDiagnosticEventCaption(),
            messageBuilder.buildPluginDiagnosticEventDescription(),
            nx::vms::api::EventLevel::ErrorEventLevel,
            m_device));

    emit pluginDiagnosticEventTriggered(pluginDiagnosticEvent);
}

void DeviceAgentHandler::handleMetadata(nx::sdk::analytics::IMetadataPacket* metadataPacket)
{
    m_metadataHandler.handleMetadata(metadataPacket);
}

void DeviceAgentHandler::handlePluginDiagnosticEvent(
    nx::sdk::IPluginDiagnosticEvent* sdkPluginDiagnosticEvent)
{
    const nx::vms::event::PluginDiagnosticEventPtr pluginDiagnosticEvent(
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

    const wrappers::DebugSettings debugSettings{
        pluginsIni().analyticsManifestOutputPath,
        pluginsIni().analyticsManifestSubstitutePath,
        nx::utils::log::Tag(this)
    };

    wrappers::ManifestProcessor<DeviceAgentManifest> manifestProcessor(
        debugSettings,
        sdkObjectDescription,
        [this, &sdkObjectDescription](wrappers::Violation violation)
        {
            const wrappers::PluginDiagnosticMessageBuilder messageBuilder(
                wrappers::SdkMethod::iHandler_pushManifest,
                sdkObjectDescription,
                violation);

            handleManifestError(messageBuilder);
        },
        [](const sdk_support::Error&) {}, //< No SDK errors can occur here.
        [this, &sdkObjectDescription](const QString& errorMessage)
        {
            const QString caption = "Internal Error in Server while receiving manifest of "
                + sdkObjectDescription.descriptionString();
            const QString description =
                errorMessage + " Additional details may be available in the Server log.";

            const wrappers::PluginDiagnosticMessageBuilder messageBuilder(
                sdkObjectDescription,
                caption,
                description);

            handleManifestError(messageBuilder);
        });

    const sdk::Ptr<const sdk::IString> manifestStringPtr =
        manifestString->queryInterface<nx::sdk::IString>();

    const std::optional<DeviceAgentManifest> manifest =
        manifestProcessor.manifestFromSdkString(manifestStringPtr);

    if (manifest && m_manifestHandler)
        m_manifestHandler(*manifest);
}

} // namespace nx::vms::server::analytics
