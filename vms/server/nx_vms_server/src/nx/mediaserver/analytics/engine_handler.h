#pragma once

#include <nx/sdk/analytics/engine.h>

#include <nx/mediaserver/resource/resource_fwd.h>
#include <nx/mediaserver/server_module_aware.h>

#include <nx/vms/event/events/events_fwd.h>

namespace nx::mediaserver::analytics {

class EngineHandler:
    public QObject,
    public nx::mediaserver::ServerModuleAware,
    public nx::sdk::analytics::Engine::IHandler
{
    Q_OBJECT

public:
    EngineHandler(
        QnMediaServerModule* serverModule,
        resource::AnalyticsEngineResourcePtr engineResource);

    virtual void handlePluginEvent(nx::sdk::IPluginEvent* pluginEvent) override;

signals:
    void pluginEventTriggered(const nx::vms::event::PluginEventPtr pluginEvent);

private:
    resource::AnalyticsEngineResourcePtr m_engineResource;
};

} // namespace nx::mediaserver::analytics
