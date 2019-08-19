#include "engine_handler.h"

#include <utils/common/synctime.h>
#include <media_server/media_server_module.h>

#include <nx/vms/event/events/events_fwd.h>
#include <nx/vms/event/events/plugin_diagnostic_event.h>

#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/sdk_support/utils.h>
#include <nx/vms/server/event/event_connector.h>

namespace nx::vms::server::analytics {

using namespace nx::vms::event;

EngineHandler::EngineHandler(
    QnMediaServerModule* serverModule,
    QnUuid engineResourceId)
    :
    ServerModuleAware(serverModule),
    m_engineResourceId(std::move(engineResourceId))
{
    connect(this, &EngineHandler::pluginDiagnosticEventTriggered,
        serverModule->eventConnector(), &event::EventConnector::at_pluginDiagnosticEvent,
        Qt::QueuedConnection);
}

void EngineHandler::handlePluginDiagnosticEvent(
    nx::sdk::IPluginDiagnosticEvent* sdkPluginDiagnosticEvent)
{
    PluginDiagnosticEventPtr pluginDiagnosticEvent(
        new PluginDiagnosticEvent(
            qnSyncTime->currentUSecsSinceEpoch(),
            m_engineResourceId,
            sdkPluginDiagnosticEvent->caption(),
            sdkPluginDiagnosticEvent->description(),
            sdk_support::fromPluginDiagnosticEventLevel(sdkPluginDiagnosticEvent->level()),
            QnVirtualCameraResourcePtr()));

    emit pluginDiagnosticEventTriggered(pluginDiagnosticEvent);
}

} // namespace nx::vms::server::analytics
