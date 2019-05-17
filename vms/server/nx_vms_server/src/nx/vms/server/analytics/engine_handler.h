#pragma once

#include <nx/sdk/analytics/i_engine.h>
#include <nx/sdk/helpers/ref_countable.h>

#include <nx/vms/server/server_module_aware.h>

#include <nx/vms/event/events/events_fwd.h>

namespace nx::vms::server::analytics {

class EngineHandler:
    public QObject,
    public /*mixin*/ nx::vms::server::ServerModuleAware,
    public nx::sdk::RefCountable<nx::sdk::analytics::IEngine::IHandler>
{
    Q_OBJECT

public:
    EngineHandler(
        QnMediaServerModule* serverModule,
        QnUuid engineResourceId);

    virtual void handlePluginEvent(nx::sdk::IPluginEvent* pluginEvent) override;

signals:
    void pluginEventTriggered(const nx::vms::event::PluginEventPtr pluginEvent);

private:
    QnUuid m_engineResourceId;
};

} // namespace nx::vms::server::analytics
