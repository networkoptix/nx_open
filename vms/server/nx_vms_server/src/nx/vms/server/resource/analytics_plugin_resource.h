#pragma once

#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/server/server_module_aware.h>

#include <nx/sdk/analytics/i_plugin.h>
#include <nx/sdk/ptr.h>

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

    void setSdkPlugin(std::shared_ptr<analytics::wrappers::Plugin> sdkPlugin);

    std::shared_ptr<analytics::wrappers::Plugin> sdkPlugin() const;

    QString libName() const;

protected:
    virtual CameraDiagnostics::Result initInternal() override;

private:
    std::shared_ptr<analytics::wrappers::Plugin> m_sdkPlugin;
};

} // namespace resource
} // namespace nx::vms::server
