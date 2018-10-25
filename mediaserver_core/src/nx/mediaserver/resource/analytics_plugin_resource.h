#pragma once

#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/mediaserver/server_module_aware.h>

#include <nx/sdk/analytics/plugin.h>

#include <nx/mediaserver/sdk_support/pointers.h>

namespace nx::mediaserver::resource {

class AnalyticsPluginResource:
    public nx::vms::common::AnalyticsPluginResource,
    public nx::mediaserver::ServerModuleAware
{
    using base_type = nx::vms::common::AnalyticsPluginResource;

public:
    AnalyticsPluginResource(QnMediaServerModule* serverModule);

    void setSdkPlugin(sdk_support::SharedPtr<nx::sdk::analytics::Plugin> plugin);

    sdk_support::SharedPtr<nx::sdk::analytics::Plugin> sdkPlugin() const;

protected:
    virtual CameraDiagnostics::Result initInternal() override;

private:
    sdk_support::SharedPtr<nx::sdk::analytics::Plugin> m_sdkPlugin;
};

} // namespace nx::mediaserver::resource
