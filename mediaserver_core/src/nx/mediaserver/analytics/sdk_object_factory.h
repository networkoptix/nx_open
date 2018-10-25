#pragma once

#include <utils/common/connective.h>
#include <core/resource/resource_fwd.h>

#include <nx/vms/api/data/analytics_data.h>

#include <nx/mediaserver/server_module_aware.h>
#include <nx/mediaserver/sdk_support/pointers.h>

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
        const QnUuid& pluginId,
        const int engineIndex) const;
};

} // namespace nx::mediaserver::analytics
