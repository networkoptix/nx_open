#pragma once

#include <nx/sdk/analytics/i_engine.h>

#include <nx/vms/server/resource/resource_fwd.h>
#include <nx/vms/server/server_module_aware.h>

#include <nx/vms/event/events/events_fwd.h>

namespace nx::vms::server::analytics {

class EngineHandler:
    public QObject,
    public nx::vms::server::ServerModuleAware,
    public nx::sdk::analytics::IEngine::IHandler
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

} // namespace nx::vms::server::analytics
