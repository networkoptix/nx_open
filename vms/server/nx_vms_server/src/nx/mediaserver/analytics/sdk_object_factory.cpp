#include "sdk_object_factory.h"

#include <nx/vms/api/analytics/plugin_manifest.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx_ec/data/api_conversion_functions.h>

#include <nx/mediaserver/sdk_support/pointers.h>
#include <nx/mediaserver/sdk_support/utils.h>

#include <nx/mediaserver/interactive_settings/json_engine.h>

#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/analytics/descriptor_list_manager.h>

#include <nx/sdk/analytics/plugin.h>
#include <nx/sdk/settings.h>
#include <plugins/settings.h>

#include <plugins/plugin_manager.h>
#include <common/common_module.h>
#include <media_server/media_server_module.h>
#include <core/resource_management/resource_pool.h>

#include <nx_ec/managers/abstract_analytics_manager.h>
#include <nx_ec/ec_api.h>

namespace nx::mediaserver::analytics {

using namespace nx::vms::common;

class SdkObjectFactory;

namespace {

using PluginPtr = sdk_support::SharedPtr<nx::sdk::analytics::Plugin>;
using EnginePtr = sdk_support::SharedPtr<nx::sdk::analytics::Engine>;

const nx::utils::log::Tag kLogTag{typeid(nx::mediaserver::analytics::SdkObjectFactory)};

PluginManager* getPluginManager(QnMediaServerModule* serverModule)
{
    if (!serverModule)
    {
        NX_ASSERT(false, "Can't access the server module");
        return nullptr;
    }

    auto pluginManager = serverModule->pluginManager();
    if (!pluginManager)
    {
        NX_ASSERT(false, "Can't access the plugin manager");
        return nullptr;
    }

    return pluginManager;
}

ec2::AbstractAnalyticsManagerPtr getAnalyticsManager(QnMediaServerModule* serverModule)
{
    if (!serverModule)
    {
        NX_ASSERT(false, "Can't access the server module");
        return nullptr;
    }

    auto commonModule = serverModule->commonModule();
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
    clearActionDescriptorList();

    if (!initPluginResources())
        return false;

    if (!initEngineResources())
        return false;

    return true;
}

bool SdkObjectFactory::clearActionDescriptorList()
{
    auto descriptorListManager = serverModule()
        ->commonModule()
        ->analyticsDescriptorListManager();

    descriptorListManager->clearDescriptors<nx::vms::api::analytics::ActionTypeDescriptor>();
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
    auto error = analyticsManager->getAnalyticsPluginsSync(&databaseAnalyticsPlugins);
    if (error != ec2::ErrorCode::ok)
        return false;

    std::map<QnUuid, nx::vms::api::AnalyticsPluginData> pluginDataById;
    for (auto& analyticsPluginData: databaseAnalyticsPlugins)
        pluginDataById.emplace(analyticsPluginData.id, std::move(analyticsPluginData));

    auto realAnalyticsPlugins = pluginManager->findNxPlugins<nx::sdk::analytics::Plugin>(
        nx::sdk::analytics::IID_Plugin);

    std::map<QnUuid, PluginPtr> sdkPluginsById;
    for (const auto realAnalyticsPlugin: realAnalyticsPlugins)
    {
        auto realAnalyticsPluginPtr =
            sdk_support::SharedPtr<nx::sdk::analytics::Plugin>(realAnalyticsPlugin);

        const auto pluginManifest = sdk_support::manifest<nx::vms::api::analytics::PluginManifest>(
            realAnalyticsPlugin,
            makeLogger(resource::AnalyticsPluginResourcePtr()));

        if (!pluginManifest)
        {
            NX_ERROR(
                this,
                "Can't fetch a manifest from the analytics plugin %1",
                realAnalyticsPlugin->name());
            continue;
        }

        const auto id = QnUuid::fromArbitraryData(pluginManifest->id);
        sdkPluginsById.emplace(id, realAnalyticsPluginPtr);

        NX_DEBUG(
            this,
            "Creating an analytics plugin resource. Id: %1; Name: %2",
            id, pluginManifest->name);

        auto& data = pluginDataById[id];
        data.id = id;
        data.typeId = nx::vms::api::AnalyticsPluginData::kResourceTypeId;
        data.parentId = QnUuid();
        data.name = pluginManifest->name;
        data.url = QString();
    }

    auto resPool = resourcePool();
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
            NX_WARNING(this, "Unable to find a SDK plugin object. Plugin name: %1, plugin Id (%2)",
                pluginData.name, pluginData.id);
            continue;
        }

        pluginResource->setSdkPlugin(sdkPluginItr->second);
        auto result = pluginResource->init();
        if (!result)
        {
            NX_WARNING(this,
                "Error while initializing plugin resource: %1. "
                "Plugin resource name: %2, Plugin resource Id: %3",
                result, pluginResource->getName(), pluginResource->getId());

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
        NX_ERROR(
            this,
            lm("Error has occured while retrieving engines from the database: %1")
                .args(errorCode));
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

    std::map<QnUuid, EnginePtr> sdkEnginesById;
    for (const auto& entry: engineDataByPlugin)
    {
        const auto& pluginId = entry.first;
        const auto& engineList = entry.second;
        for (const auto& engine : engineList)
        {
            analyticsManager->saveSync(engine);
            auto engineResource = resourcePool()
                ->getResourceById<resource::AnalyticsEngineResource>(engine.id);

            if (!engineResource)
            {
                NX_WARNING(
                    this,
                    "Unable to find an analytics engine resource in the resource pool. "
                    "Engine name: %1, engine Id: (%2)",
                    engine.name, engine.id);

                continue;
            }

            auto parentPlugin = engineResource->plugin()
                .dynamicCast<resource::AnalyticsPluginResource>();

            if (!parentPlugin)
            {
                NX_WARNING(
                    this,
                    "Unable to find a parent analytics plugin for the engine %1 (%2)",
                    engineResource->getName(), engineResource->getId());

                continue;
            }

            auto sdkPlugin = parentPlugin->sdkPlugin();
            if (!sdkPlugin)
            {
                NX_WARNING(
                    this,
                    "Plugin resource %1 (%2) has no correspondent SDK object",
                    parentPlugin->getName(), parentPlugin->getId());

                continue;
            }

            nx::sdk::Error error = nx::sdk::Error::noError;
            EnginePtr sdkEngine(sdkPlugin->createEngine(&error));

            if (!sdkEngine)
            {
                NX_WARNING(this, "Unable to create a SDK engine %1 (%2)",
                    engineResource->getName(), engineResource->getId());
                continue;
            }

            if (error != nx::sdk::Error::noError)
            {
                NX_WARNING(this,
                    "Error '%1' occured while creating a SDK engine. "
                    "Engine resource name: %2, engine resource Id: %3",
                    error, engineResource->getName(), engineResource->getId());

                continue;
            }

            engineResource->setSdkEngine(sdkEngine);
            const auto result = engineResource->init();
            if (!result)
            {
                NX_WARNING(this,
                    "Error while initializing engine resource: %1. "
                    "Engine resource name: %2, engine resource Id: %3",
                    result, engineResource->getName(), engineResource->getId());

                continue;
            }
            engineResource->setStatus(Qn::Online);
        }
    }

    return true;
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

std::unique_ptr<sdk_support::AbstractManifestLogger> SdkObjectFactory::makeLogger(
    resource::AnalyticsPluginResourcePtr pluginResource) const
{
    const QString messageTemplate(
        "Error occurred while fetching Plugin manifest: {:error}");

    return std::make_unique<sdk_support::ManifestLogger>(
        nx::utils::log::Tag(typeid(this)),
        messageTemplate,
        std::move(pluginResource));
}

} // namespace nx::mediaserver::analytics
