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

class SdkObjectPool:
    public Connective<QObject>,
    public nx::mediaserver::ServerModuleAware
{
    Q_OBJECT

    using base_type = nx::mediaserver::ServerModuleAware;
    using PluginPtr = sdk_support::SharedPtr<nx::sdk::analytics::Plugin>;
    using EnginePtr = sdk_support::SharedPtr<nx::sdk::analytics::Engine>;

    struct EngineDescriptor
    {
        EnginePtr engine;
        PluginPtr plugin;

        EngineDescriptor() = default;
        EngineDescriptor(EnginePtr enginePtr, PluginPtr pluginPtr):
            engine(std::move(enginePtr)),
            plugin(std::move(pluginPtr))
        {
        }
    };

public:
    SdkObjectPool(QnMediaServerModule* serverModule);
    bool init();

    PluginPtr plugin(const QnUuid& id) const;
    EnginePtr engine(const QnUuid& id) const;

    EnginePtr instantiateEngine(const QnUuid& engineId, const QJsonObject& settings);

private:
    bool initPluginResources();
    bool initEngineResources();

    nx::vms::api::AnalyticsEngineData createEngineData(
        const QnUuid& pluginId,
        const int engineIndex) const;

    PluginPtr pluginUnsafe(const QnUuid& id) const;
    std::shared_ptr<EngineDescriptor> engineDescriptorUnsafe(const QnUuid& id) const;


private:
    mutable QnMutex m_mutex;

    std::map<QnUuid, PluginPtr> m_plugins;
    std::map<QnUuid, std::shared_ptr<EngineDescriptor>> m_engines;
};

} // namespace nx::mediaserver::analytics
