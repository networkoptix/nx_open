#pragma once

#include <utils/common/connective.h>
#include <core/resource/resource_fwd.h>
#include <nx/mediaserver/resource/resource_fwd.h>

#include <nx/vms/api/data/analytics_data.h>

#include <nx/mediaserver/server_module_aware.h>
#include <nx/mediaserver/sdk_support/pointers.h>
#include <nx/mediaserver/sdk_support/loggers.h>

namespace nx::sdk::analytics {

class Plugin;
class Engine;

} // namespace nx::sdk::analytics

namespace nx::mediaserver::analytics {

class SdkObjectFactory:
    public Connective<QObject>,
    public nx::mediaserver::ServerModuleAware
{
    Q_OBJECT

    using base_type = nx::mediaserver::ServerModuleAware;

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
        const nx::sdk::analytics::Plugin* plugin) const;
};

} // namespace nx::mediaserver::analytics
