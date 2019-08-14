#include "sdk_object_factory.h"

#include <nx/vms/api/analytics/plugin_manifest.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx_ec/data/api_conversion_functions.h>

#include <nx/sdk/ptr.h>
#include <nx/vms/server/sdk_support/utils.h>
#include <nx/vms/server/sdk_support/to_string.h>

#include <nx/vms/server/analytics/wrappers/plugin.h>
#include <nx/vms/server/analytics/wrappers/engine.h>
#include <nx/vms/api/analytics/plugin_manifest.h>
#include <nx/vms/api/analytics/engine_manifest.h>

#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>

#include <nx/analytics/descriptor_manager.h>

#include <nx/sdk/analytics/i_plugin.h>
#include <plugins/settings.h>

#include <plugins/plugin_manager.h>
#include <common/common_module.h>
#include <media_server/media_server_module.h>
#include <core/resource_management/resource_pool.h>
#include <api/runtime_info_manager.h>

#include <nx_ec/managers/abstract_analytics_manager.h>
#include <nx_ec/ec_api.h>

namespace nx::vms::server::analytics {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

template<typename T>
using ResultHolder = nx::vms::server::sdk_support::ResultHolder<T>;

class SdkObjectFactory;

namespace {

const nx::utils::log::Tag kLogTag{typeid(nx::vms::server::analytics::SdkObjectFactory)};

PluginManager* getPluginManager(QnMediaServerModule* serverModule)
{
    if (!NX_ASSERT(serverModule, "Can't access the server module"))
        return nullptr;

    const auto pluginManager = serverModule->pluginManager();
    NX_ASSERT(pluginManager, "Can't access the plugin manager");
    return pluginManager;
}

ec2::AbstractAnalyticsManagerPtr getAnalyticsManager(QnMediaServerModule* serverModule)
{
    if (!serverModule)
    {
        NX_ASSERT(false, "Can't access the server module");
        return nullptr;
    }

    const auto commonModule = serverModule->commonModule();
    if (!commonModule)
    {
        NX_ASSERT(false, "Can't access the common module");
        return nullptr;
    }

    auto ec2Connection = commonModule->ec2Connection();
    if (!ec2Connection)
    {
        NX_ASSERT(false, "Can't access the ec2 connection");
        return nullptr;
    }

    auto analyticsManager = ec2Connection->getAnalyticsManager(Qn::kSystemAccess);
    if (!analyticsManager)
    {
        NX_ASSERT(false, "Can't access the analytics manager");
        return nullptr;
    }

    return analyticsManager;
}

QnUuid defaultEngineId(const QnUuid& pluginId)
{
    return QnUuid::fromArbitraryData("engine_" + pluginId.toString());
}

} // namespace

SdkObjectFactory::SdkObjectFactory(QnMediaServerModule* serverModule):
    base_type(serverModule)
{
}

bool SdkObjectFactory::init()
{
    if (!initPluginResources())
        return false;

    if (!initEngineResources())
        return false;

    return true;
}

bool SdkObjectFactory::initPluginResources()
{
    auto pluginManager = getPluginManager(serverModule());
    if (!pluginManager)
        return false;

    auto analyticsManager = getAnalyticsManager(serverModule());
    if (!analyticsManager)
        return false;

    nx::vms::api::AnalyticsPluginDataList databaseAnalyticsPlugins;
    if (analyticsManager->getAnalyticsPluginsSync(&databaseAnalyticsPlugins) != ec2::ErrorCode::ok)
        return false;

    std::map<QnUuid, nx::vms::api::AnalyticsPluginData> pluginDataById;
    for (auto& analyticsPluginData: databaseAnalyticsPlugins)
        pluginDataById.emplace(analyticsPluginData.id, std::move(analyticsPluginData));

    const auto analyticsPlugins = pluginManager->findNxPlugins<nx::sdk::analytics::IPlugin>();

    std::map<QnUuid, Ptr<nx::sdk::analytics::IPlugin>> sdkPluginsById;
    for (const auto& analyticsPlugin: analyticsPlugins)
    {
        const auto pluginInfo = pluginManager->pluginInfo(analyticsPlugin.get());
        if (!NX_ASSERT(pluginInfo))
            continue;

        const QString pluginLibName = pluginInfo->libName;
        const auto pluginWrapper = std::make_shared<wrappers::Plugin>(
            serverModule(),
            analyticsPlugin,
            pluginLibName);

        const auto pluginManifest = pluginWrapper->manifest();
        if (!pluginManifest)
            continue;

        const auto id = QnUuid::fromArbitraryData(pluginManifest->id);
        sdkPluginsById.emplace(id, analyticsPlugin);

        NX_DEBUG(this, "Creating an analytics plugin resource. Id: %1; Name: %2",
            id, pluginManifest->name);

        auto& data = pluginDataById[id];
        data.id = id;
        data.typeId = nx::vms::api::AnalyticsPluginData::kResourceTypeId;
        data.parentId = QnUuid();
        data.name = pluginManifest->name;
        data.url = QString();
    }

    const auto resPool = resourcePool();
    if (!resPool)
    {
        NX_ERROR(this, "Can't access the resource pool");
        return false;
    }

    for (const auto& entry: pluginDataById)
    {
        const auto& pluginData = entry.second;
        analyticsManager->saveSync(pluginData);
        auto pluginResource = resPool->getResourceById<resource::AnalyticsPluginResource>(
            pluginData.id);

        if (!pluginResource)
        {
            NX_WARNING(this,
                "Unable to find a plugin resource in the resource pool. "
                "Plugin name: %1, plugin Id: %2",
                pluginData.name, pluginData.id);
            continue;
        }

        auto sdkPluginItr = sdkPluginsById.find(pluginData.id);
        if (sdkPluginItr == sdkPluginsById.cend())
        {
            NX_WARNING(this,
                "Unable to find an SDK plugin object. Plugin name: %1, plugin Id (%2)",
                pluginData.name, pluginData.id);
            continue;
        }

        const auto pluginInfo = pluginManager->pluginInfo(sdkPluginItr->second.get());
        if (!NX_ASSERT(pluginInfo))
            continue;

        pluginResource->setSdkPlugin(std::make_shared<wrappers::Plugin>(
            serverModule(), pluginResource, sdkPluginItr->second, pluginInfo->libName));

        const bool result = pluginResource->init();
        if (!result)
        {
            NX_WARNING(this, "Error while initializing plugin resource %1", pluginResource);
            continue;
        }
        pluginResource->setStatus(Qn::Online);
    }

    return true;
}

bool SdkObjectFactory::initEngineResources()
{
    auto analyticsManager = getAnalyticsManager(serverModule());
    if (!analyticsManager)
        return false;

    nx::vms::api::AnalyticsEngineDataList databaseAnalyticsEngines;
    const auto errorCode = analyticsManager->getAnalyticsEnginesSync(&databaseAnalyticsEngines);

    if (errorCode != ec2::ErrorCode::ok)
    {
        NX_ERROR(this, "Error while retrieving Engines from the database: %1", errorCode);
        return false;
    }

    std::map<QnUuid, std::vector<nx::vms::api::AnalyticsEngineData>> engineDataByPlugin;
    for (const auto& databaseAnalyticsEngine : databaseAnalyticsEngines)
    {
        engineDataByPlugin[databaseAnalyticsEngine.parentId]
            .push_back(std::move(databaseAnalyticsEngine));
    }

    auto pluginResources = resourcePool()->getResources<resource::AnalyticsPluginResource>();
    for (const auto& plugin: pluginResources)
    {
        const auto& pluginId = plugin->getId();
        auto& pluginEngineList = engineDataByPlugin[pluginId];

        // TODO: Decide on a proper creation policy for Engines.
        if (pluginEngineList.empty())
            pluginEngineList.push_back(createEngineData(plugin, defaultEngineId(plugin->getId())));
    }

    QSet<QnUuid> activeEngines;
    std::map<QnUuid, Ptr<IEngine>> sdkEnginesById;
    for (const auto& entry: engineDataByPlugin)
    {
        const auto& engineDataList = entry.second;
        for (const auto& engineData: engineDataList)
        {
            analyticsManager->saveSync(engineData);
            if (const auto engineResource =
                resourcePool()->getResourceById<resource::AnalyticsEngineResource>(engineData.id))
            {
                if (createEngine(engineResource))
                    activeEngines.insert(engineResource->getId());
            }
            else
            {
                NX_WARNING(this,
                    "Unable to find Analytics Engine Resource in Resource Pool for Engine %1 (%2)",
                    engineData.name, engineData.id);
            }
        }
    }

    updateActiveEngines(std::move(activeEngines));
    return true;
}

bool SdkObjectFactory::createEngine(
    const resource::AnalyticsEngineResourcePtr& engineResource) const
{
    const auto error = //< Allows to be called as `return error("%1...", args);`.
        [this](auto&&... args)
        {
            NX_WARNING(this, std::forward<decltype(args)>(args)...);
            return false;
        };

    const QString engineRef =
        lm("Engine %1 (%2)").args(engineResource->getName(), engineResource->getId());

    const auto pluginResource =
        engineResource->plugin().dynamicCast<resource::AnalyticsPluginResource>();

    if (!pluginResource)
        return error("Unable to find a parent Analytics Plugin for the %1", engineRef);

    const auto plugin = pluginResource->sdkPlugin();
    if (!plugin)
    {
        return error("Plugin Resource %1 (%2) has no corresponding Plugin object",
            pluginResource->getName(), pluginResource->getId());
    }

    const wrappers::EnginePtr engine = plugin->createEngine(engineResource);
    if (!engine)
        return false;

    engineResource->setSdkEngine(engine);
    if (!engineResource->init())
        return error("Error while initializing %1", engineRef);

    if (engineResource->isDeviceDependent())
    {
        const auto pluginManager = getPluginManager(serverModule());
        if (!pluginManager)
            return false; //< An assertion already has failed.

        const auto sdkPluginObject = plugin->sdkObject();
        if (!sdkPluginObject)
            return error("Unable to access SDK Plugin object of %1", engineRef);

        pluginManager->setIsActive(sdkPluginObject.get(), /*isActive*/ false);
    }

    return true;
}

void SdkObjectFactory::updateActiveEngines(QSet<QnUuid> activeEngines)
{
    auto runtimeInfoManager = serverModule()->commonModule()->runtimeInfoManager();
    auto localRuntimeInfo = runtimeInfoManager->localInfo();

    localRuntimeInfo.data.activeAnalyticsEngines = std::move(activeEngines);
    runtimeInfoManager->updateLocalItem(localRuntimeInfo);
}

nx::vms::api::AnalyticsEngineData SdkObjectFactory::createEngineData(
    const resource::AnalyticsPluginResourcePtr& plugin,
    const QnUuid& engineId) const
{
    using namespace nx::vms::api;

    const auto pluginId = plugin->getId();
    const auto pluginName = plugin->getName();

    AnalyticsEngineData engineData;
    engineData.id = engineId;
    engineData.parentId = pluginId;
    engineData.name = pluginName;
    engineData.typeId = nx::vms::api::AnalyticsEngineData::kResourceTypeId;

    return engineData;
}

} // namespace nx::vms::server::analytics
