#include "engine_handler.h"

#include <utils/common/synctime.h>
#include <media_server/media_server_module.h>

#include <nx/vms/event/events/events_fwd.h>
#include <nx/vms/event/events/plugin_event.h>

#include <nx/mediaserver/resource/analytics_engine_resource.h>
#include <nx/mediaserver/sdk_support/utils.h>
#include <nx/mediaserver/event/event_connector.h>

namespace nx::mediaserver::analytics {

using namespace nx::vms::event;

EngineHandler::EngineHandler(
    QnMediaServerModule* serverModule,
    nx::mediaserver::resource::AnalyticsEngineResourcePtr engineResource)
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
            m_engineResource->getId(),
            sdkPluginEvent->caption(),
            sdkPluginEvent->description(),
            nx::mediaserver::sdk_support::fromSdkPluginEventLevel(sdkPluginEvent->level()),
            QStringList()));

    emit pluginEventTriggered(pluginEvent);
}

} // namespace nx::mediaserver::analytlics
