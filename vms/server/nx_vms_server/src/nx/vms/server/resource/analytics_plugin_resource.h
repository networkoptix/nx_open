#pragma once

#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/server/analytics/wrappers/fwd.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>

namespace nx::vms::server {

namespace analytics::wrappers { class Plugin; }

namespace resource {

class AnalyticsPluginResource:
    public nx::vms::common::AnalyticsPluginResource,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
    using base_type = nx::vms::common::AnalyticsPluginResource;

public:
    AnalyticsPluginResource(QnMediaServerModule* serverModule);

    void setSdkPlugin(analytics::wrappers::PluginPtr sdkPlugin);

    analytics::wrappers::PluginPtr sdkPlugin() const;

    QString libName() const;

protected:
    virtual CameraDiagnostics::Result initInternal() override;

private:
    analytics::wrappers::PluginPtr m_sdkPlugin;
};

} // namespace resource
} // namespace nx::vms::server
