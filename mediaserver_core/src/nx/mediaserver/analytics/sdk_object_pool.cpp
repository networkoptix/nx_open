#include "sdk_object_pool.h"

#include <nx/vms/api/analytics/plugin_manifest.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx_ec/data/api_conversion_functions.h>

#include <nx/mediaserver/sdk_support/pointers.h>
#include <nx/mediaserver/sdk_support/utils.h>

#include <nx/mediaserver/interactive_settings/json_engine.h>

#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>

#include <nx/sdk/analytics/plugin.h>
#include <nx/plugins/settings.h>

#include <plugins/plugin_manager.h>
#include <common/common_module.h>
#include <media_server/media_server_module.h>
#include <core/resource_management/resource_pool.h>

#include <nx_ec/managers/abstract_analytics_manager.h>
#include <nx_ec/ec_api.h>

namespace nx::mediaserver::analytics {

using namespace nx::vms::common;
namespace analytics_sdk = nx::sdk::analytics;
namespace analytics_api = nx::vms::api::analytics;

class SdkObjectPool;

namespace {

const nx::utils::log::Tag kLogTag{typeid(nx::mediaserver::analytics::SdkObjectPool)};

PluginManager* getPluginManager(QnMediaServerModule* serverModule)
{
    if (!serverModule)
    {
        NX_ASSERT(false, "Can't access server module");
        return nullptr;
    }

    auto pluginManager = serverModule->pluginManager();
    if (!pluginManager)
    {
        NX_ASSERT(false, "Can't access plugin manager");
        return nullptr;
    }

    return pluginManager;
}

ec2::AbstractAnalyticsManagerPtr getAnalyticsManager(QnMediaServerModule* serverModule)
{
    if (!serverModule)
    {
        NX_ASSERT(false, "Can't access server module");
        return nullptr;
    }

    auto commonModule = serverModule->commonModule();
    if (!commonModule)
    {
        NX_ASSERT(false, "Can't access common module");
        return nullptr;
    }

    auto ec2Connection = commonModule->ec2Connection();
    if (!ec2Connection)
    {
        NX_ASSERT(false, "Can't access ec2 connection");
        return nullptr;
    }

    auto analyticsManager = ec2Connection->getAnalyticsManager(Qn::kSystemAccess);
    if (!analyticsManager)
    {
        NX_ASSERT(false, "Can't access analytics manager");
        return nullptr;
    }

    return analyticsManager;
}

QnUuid engineId(const QnUuid& pluginId, int engineIndex)
{
    return QnUuid::fromArbitraryData(
        "engine_" + QString::number(engineIndex) + "_" + pluginId.toString());
}

} // namespace

SdkObjectPool::SdkObjectPool(QnMediaServerModule* serverModule):
    base_type(serverModule)
{
}

bool SdkObjectPool::init()
{
    if (!initPluginResources())
        return false;

    if (!initEngineResources())
        return false;

    return true;
}

bool SdkObjectPool::initPluginResources()
{
    auto pluginManager = getPluginManager(serverModule());
    if (!pluginManager)
        return false;

    auto analyticsManager = getAnalyticsManager(serverModule());
    if (!analyticsManager)
        return false;

    ec2::ErrorCode error;
    nx::vms::api::AnalyticsPluginDataList databaseAnalyticsPlugins;
    error = analyticsManager->getAnalyticsPluginsSync(&databaseAnalyticsPlugins);
    if (error != ec2::ErrorCode::ok)
        return false;

    std::map<QnUuid, nx::vms::api::AnalyticsPluginData> pluginDataById;
    for (auto& analyticsPluginData: databaseAnalyticsPlugins)
        pluginDataById.emplace(analyticsPluginData.id, std::move(analyticsPluginData));

    auto analyticsPlugins = pluginManager->findNxPlugins<analytics_sdk::Plugin>(
        nx::sdk::analytics::IID_Plugin);

    for (const auto analyticsPlugin: analyticsPlugins)
    {
        auto analyticsPluginPtr = sdk_support::SharedPtr<analytics_sdk::Plugin>(analyticsPlugin);
        const auto manifest = sdk_support::manifest<analytics_api::PluginManifest>(
            analyticsPlugin);

        const auto id = QnUuid::fromArbitraryData(manifest->id);

        {
            QnMutexLocker lock(&m_mutex);
            m_plugins.emplace(id, analyticsPluginPtr);
        }

        if (!manifest)
        {
            NX_ERROR(this, "Got broken manifest");
            continue;
        }

        NX_DEBUG(
            this,
            lm("Analytics creating plugin resource. Id: %1; Name: %2").args(id, manifest->name));

        auto& data = pluginDataById[id];
        data.id = id;
        data.typeId = nx::vms::api::AnalyticsPluginData::kResourceTypeId;
        data.parentId = QnUuid();
        data.name = manifest->name;
        data.url = QString();
    }

    for (const auto& entry: pluginDataById)
    {
        const auto& pluginData = entry.second;
        analyticsManager->saveSync(pluginData);
    }

    return true;
}

bool SdkObjectPool::initEngineResources()
{
    auto analyticsManager = getAnalyticsManager(serverModule());
    if (!analyticsManager)
        return false;

    nx::vms::api::AnalyticsEngineDataList engines;
    const auto errorCode = analyticsManager->getAnalyticsEnginesSync(&engines);

    if (errorCode != ec2::ErrorCode::ok)
    {
        NX_ERROR(
            this,
            lm("Error has occured while retrieving engines from database: %1").args(errorCode));
        return false;
    }

    std::map<QnUuid, std::vector<nx::vms::api::AnalyticsEngineData>> enginesByPlugin;
    for (const auto& engine: engines)
        enginesByPlugin[engine.parentId].push_back(std::move(engine));

    for (const auto& entry: m_plugins)
    {
        const auto& pluginId = entry.first;
        auto& pluginEngineList = enginesByPlugin[pluginId];

        // TODO: Create default engine only if plugin has correspondent capability.
        if (pluginEngineList.empty())
            pluginEngineList.push_back(createEngineData(pluginId, /*engineIndex*/ 0));
    }

    for (const auto& entry: enginesByPlugin)
    {
        const auto& pluginId = entry.first;
        const auto& engineList = entry.second;
        for (const auto& engine: engineList)
        {
            analyticsManager->saveSync(engine);

            {
                QnMutexLocker lock(&m_mutex);
                m_engines.emplace(
                    engine.id,
                    std::make_shared<EngineDescriptor>(EnginePtr(), pluginUnsafe(pluginId)));
            }
        }
    }

    return true;
}

nx::vms::api::AnalyticsEngineData SdkObjectPool::createEngineData(
    const QnUuid& pluginId,
    int engineIndex) const
{
    using namespace nx::vms::api;
    AnalyticsEngineData engineData;
    engineData.id = engineId(pluginId, engineIndex);
    engineData.parentId = pluginId;

    // TODO: Plugin name should be used here.
    engineData.name = "Engine " + QString::number(engineIndex);
    engineData.typeId = nx::vms::api::AnalyticsEngineData::kResourceTypeId;

    return engineData;
}

SdkObjectPool::PluginPtr SdkObjectPool::plugin(const QnUuid & id) const
{
    QnMutexLocker lock(&m_mutex);
    return pluginUnsafe(id);
}

SdkObjectPool::PluginPtr SdkObjectPool::pluginUnsafe(const QnUuid& id) const
{
    auto itr = m_plugins.find(id);
    return itr == m_plugins.cend() ? nullptr : itr->second;
}

SdkObjectPool::EnginePtr SdkObjectPool::engine(const QnUuid& id) const
{
    QnMutexLocker lock(&m_mutex);
    auto descriptor = engineDescriptorUnsafe(id);
    if (!descriptor)
        return EnginePtr();

    return descriptor->engine;
}

std::shared_ptr<SdkObjectPool::EngineDescriptor> SdkObjectPool::engineDescriptorUnsafe(
    const QnUuid& id) const
{
    auto itr = m_engines.find(id);
    if (itr == m_engines.cend())
        return nullptr;

    return itr->second;
}

SdkObjectPool::EnginePtr SdkObjectPool::instantiateEngine(
    const QnUuid& engineId,
    const QJsonObject& settings)
{
    std::shared_ptr<EngineDescriptor> descriptor;
    PluginPtr sdkPlugin = nullptr;
    EnginePtr sdkEngine = nullptr;

    {
        QnMutexLocker lock(&m_mutex);
        descriptor = engineDescriptorUnsafe(engineId);
        if (!descriptor)
        {
            NX_ERROR(this, lm("An engine with the engine Id %1 is not registered").args(engineId));
            return EnginePtr();
        }

        if (!descriptor->plugin)
        {
            NX_ERROR(this, lm("Can't determine a plugin the engine %1 belongs to").args(engineId));
            return EnginePtr();
        }

        if (descriptor->engine)
        {
            NX_WARNING(this, lm("An engine with Id %1 already exists").args(engineId));
            return descriptor->engine;
        }

        sdkEngine = descriptor->engine;
        sdkPlugin = descriptor->plugin;
    }

    if (!sdkEngine)
    {
        NX_DEBUG(this, lm("Instanciating engine with Id %1").args(engineId));
        nx::sdk::Error error;
        sdkEngine = EnginePtr(sdkPlugin->createEngine(&error));

        if (error != nx::sdk::Error::noError || !sdkEngine)
        {
            NX_ERROR(this, lm("An error occured while creating the engine %1: %2")
                .args(engineId, error));

            return EnginePtr();
        }
    }

    interactive_settings::JsonEngine jsonEngine;
    jsonEngine.load(settings);
    const auto values = jsonEngine.values();

    QMap<QString, QString> settingsMap;
    for (auto itr = values.cbegin(); itr != values.cend(); ++itr)
        settingsMap[itr.key()] = itr.value().toString();

    nx::plugins::SettingsHolder settingsHolder(settingsMap);
    sdkEngine->setSettings(settingsHolder.array(), settingsHolder.size());

    {
        QnMutexLocker lock(&m_mutex);
        descriptor->engine = sdkEngine;
    }

    return sdkEngine;
}

} // namespace nx::mediaserver::analytics
