#pragma once

#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/mediaserver/server_module_aware.h>

namespace nx::mediaserver::resource {

class AnalyticsPluginResource:
    public nx::vms::common::AnalyticsPluginResource,
    public nx::mediaserver::ServerModuleAware
{
    using base_type = nx::vms::common::AnalyticsPluginResource;

public:
    AnalyticsPluginResource(QnMediaServerModule* serverModule);

protected:
    virtual CameraDiagnostics::Result initInternal() override;
};

} // namespace nx::mediaserver::resource
