#pragma once

#include <utils/common/connective.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/server/resource/resource_fwd.h>

#include <nx/vms/api/data/analytics_data.h>

#include <nx/vms/server/server_module_aware.h>

namespace nx::sdk::analytics { class IPlugin; }

namespace nx::vms::server::analytics {

class SdkObjectFactory:
    public Connective<QObject>,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
    Q_OBJECT

    using base_type = nx::vms::server::ServerModuleAware;

public:
    SdkObjectFactory(QnMediaServerModule* serverModule);
    bool init();

private:
    bool initPluginResources();
    bool initEngineResources();

    bool createEngine(
        const resource::AnalyticsEngineResourcePtr& engineResource) const;

    void updateActiveEngines(QSet<QnUuid> activeEngines);

    nx::vms::api::AnalyticsEngineData createEngineData(
        const resource::AnalyticsPluginResourcePtr& plugin,
        const QnUuid& engineId) const;
};

} // namespace nx::vms::server::analytics
