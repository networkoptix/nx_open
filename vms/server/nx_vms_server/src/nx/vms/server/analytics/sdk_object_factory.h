#pragma once

#include <utils/common/connective.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/server/resource/resource_fwd.h>

#include <nx/vms/api/data/analytics_data.h>

#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/server/sdk_support/loggers.h>

namespace nx::sdk::analytics { class IPlugin; }

namespace nx::vms::server::analytics {

class SdkObjectFactory:
    public Connective<QObject>,
    public nx::vms::server::ServerModuleAware
{
    Q_OBJECT

    using base_type = nx::vms::server::ServerModuleAware;

public:
    SdkObjectFactory(QnMediaServerModule* serverModule);
    bool init();

private:
    bool clearActionDescriptorList();
    bool initPluginResources();
    bool initEngineResources();

    nx::vms::api::AnalyticsEngineData createEngineData(
        const resource::AnalyticsPluginResourcePtr& plugin,
        const QnUuid& engineId) const;

    std::unique_ptr<sdk_support::AbstractManifestLogger> makeLogger(
        resource::AnalyticsPluginResourcePtr pluginResource) const;

    std::unique_ptr<sdk_support::AbstractManifestLogger> makeLogger(
        const nx::sdk::analytics::IPlugin* plugin) const;
};

} // namespace nx::vms::server::analytics
