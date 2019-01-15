#include "engine_handler.h"

#include <utils/common/synctime.h>
#include <media_server/media_server_module.h>

#include <nx/vms/event/events/events_fwd.h>
#include <nx/vms/event/events/plugin_event.h>

#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/sdk_support/utils.h>
#include <nx/vms/server/event/event_connector.h>

namespace nx::vms::server::analytics {

using namespace nx::vms::event;

EngineHandler::EngineHandler(
    QnMediaServerModule* serverModule,
    nx::vms::server::resource::AnalyticsEngineResourcePtr engineResource)
    :
    ServerModuleAware(serverModule),
    m_engineResource(std::move(engineResource))
{
    connect(this, &EngineHandler::pluginEventTriggered,
        serverModule->eventConnector(), &event::EventConnector::at_pluginEvent,
        Qt::QueuedConnection);
}

void EngineHandler::handlePluginEvent(nx::sdk::IPluginEvent* sdkPluginEvent)
{
    nx::vms::event::PluginEventPtr pluginEvent(
        new PluginEvent(
            qnSyncTime->currentUSecsSinceEpoch(),
            m_engineResource,
            sdkPluginEvent->caption(),
            sdkPluginEvent->description(),
            nx::vms::server::sdk_support::fromSdkPluginEventLevel(sdkPluginEvent->level()),
            QnSecurityCamResourcePtr()));

    emit pluginEventTriggered(pluginEvent);
}

} // namespace nx::vms::server::analytlics
