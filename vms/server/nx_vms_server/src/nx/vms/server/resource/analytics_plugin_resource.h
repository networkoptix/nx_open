#pragma once

#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/server/server_module_aware.h>

#include <nx/sdk/analytics/i_plugin.h>
#include <nx/sdk/helpers/ptr.h>

#include <nx/vms/server/sdk_support/loggers.h>

namespace nx::vms::server::resource {

class AnalyticsPluginResource:
    public nx::vms::common::AnalyticsPluginResource,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
    using base_type = nx::vms::common::AnalyticsPluginResource;

public:
    AnalyticsPluginResource(QnMediaServerModule* serverModule);

    void setSdkPlugin(nx::sdk::Ptr<nx::sdk::analytics::IPlugin> plugin);

    nx::sdk::Ptr<nx::sdk::analytics::IPlugin> sdkPlugin() const;

private:
    std::unique_ptr<sdk_support::AbstractManifestLogger> makeLogger() const;

protected:
    virtual CameraDiagnostics::Result initInternal() override;

private:
    nx::sdk::Ptr<nx::sdk::analytics::IPlugin> m_sdkPlugin;
};

} // namespace nx::vms::server::resource
