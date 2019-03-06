#include "analytics_engine_resource.h"

#include <media_server/media_server_module.h>
#include <plugins/plugins_ini.h>

#include <nx/vms/server/sdk_support/utils.h>
#include <nx/vms/server/analytics/debug_helpers.h>
#include <nx/vms/server/interactive_settings/json_engine.h>
#include <nx/vms/server/analytics/debug_helpers.h>
#include <nx/vms/api/analytics/descriptors.h>

#include <nx/sdk/analytics/i_plugin.h>
#include <nx/sdk/analytics/helpers/engine_info.h>
#include <nx/sdk/helpers/to_string.h>
#include <nx/sdk/i_string_map.h>
#include <nx/utils/member_detector.h>
#include <nx/analytics/descriptor_manager.h>

namespace nx::vms::server::resource {

AnalyticsEngineResource::AnalyticsEngineResource(QnMediaServerModule* serverModule):
    base_type(),
    ServerModuleAware(serverModule)
{
}

void AnalyticsEngineResource::setSdkEngine(
    nx::sdk::Ptr<nx::sdk::analytics::IEngine> sdkEngine)
{
    m_sdkEngine = std::move(sdkEngine);
}

nx::sdk::Ptr<nx::sdk::analytics::IEngine> AnalyticsEngineResource::sdkEngine() const
{
    return m_sdkEngine;
}

QVariantMap AnalyticsEngineResource::settingsValues() const
{
    auto settingsFromProperty = QJsonDocument::fromJson(
        getProperty(kSettingsValuesProperty).toUtf8()).object().toVariantMap();

    auto parentPluginManifest = pluginManifest();
    if (!parentPluginManifest)
        return settingsFromProperty;

    interactive_settings::JsonEngine jsonEngine;
    jsonEngine.loadModelFromJsonObject(parentPluginManifest->engineSettingsModel);
    jsonEngine.applyValues(settingsFromProperty);
    return jsonEngine.values();
}

void AnalyticsEngineResource::setSettingsValues(const QVariantMap& values)
{
    auto settingsFromProperty = QJsonDocument::fromJson(
        getProperty(kSettingsValuesProperty).toUtf8()).object().toVariantMap();

    auto parentPluginManifest = pluginManifest();
    if (!NX_ASSERT(parentPluginManifest, lm("Engine: %1 (%2)").args(getName(), getId())))
        return; //< TODO: Consider making this method returning bool or some kind of error.

    interactive_settings::JsonEngine jsonEngine;
    jsonEngine.loadModelFromJsonObject(parentPluginManifest->engineSettingsModel);
    jsonEngine.applyValues(settingsFromProperty);
    jsonEngine.applyValues(values);

    setProperty(kSettingsValuesProperty,
        QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(values)).toJson()));
}

bool AnalyticsEngineResource::sendSettingsToSdkEngine()
{
    auto engine = sdkEngine();
    if (!NX_ASSERT(engine, lm("Engine: %1 (%2)").args(getName(), getId())))
        return false;

    NX_DEBUG(this, "Sending settings to the Engine %1 (%2)", getName(), getId());

    nx::sdk::Ptr<nx::sdk::IStringMap> effectiveSettings;
    if (pluginsIni().analyticsEngineSettingsPath[0] != '\0')
    {
        NX_WARNING(this, "Trying to load settings for the Engine from the file. Engine %1 (%2)",
            getName(), getId());

        effectiveSettings =
            analytics::debug_helpers::loadEngineSettingsFromFile(toSharedPointer(this));
    }

    if (!effectiveSettings)
        effectiveSettings = sdk_support::toIStringMap(settingsValues());

    if (!NX_ASSERT(effectiveSettings, lm("Engine %1 (%2)").args(getName(), getId())))
        return false;

    if (pluginsIni().analyticsSettingsOutputPath[0] != '\0')
    {
        analytics::debug_helpers::dumpStringToFile(
            this,
            QString::fromStdString(nx::sdk::toJsonString(effectiveSettings.get())),
            pluginsIni().analyticsSettingsOutputPath,
            analytics::debug_helpers::nameOfFileToDumpOrLoadData(
                QnVirtualCameraResourcePtr(),
                toSharedPointer(this),
                nx::vms::server::resource::AnalyticsPluginResourcePtr(),
                "_effective_settings.json"));
    }

    engine->setSettings(effectiveSettings.get());
    return true;
}

std::optional<nx::vms::api::analytics::PluginManifest>
    AnalyticsEngineResource::pluginManifest() const
{
    auto parentPlugin = plugin().dynamicCast<resource::AnalyticsPluginResource>();
    if (!parentPlugin)
        return std::nullopt;

    return parentPlugin->manifest();
}

std::unique_ptr<sdk_support::AbstractManifestLogger> AnalyticsEngineResource::makeLogger() const
{
    const QString messageTemplate(
        "Error occurred while fetching Engine manifest for engine: {:engine}: {:error}");

    return std::make_unique<sdk_support::ManifestLogger>(
        typeid(this), //< Using the same tag for all instances.
        messageTemplate,
        toSharedPointer(this));
}

CameraDiagnostics::Result AnalyticsEngineResource::initInternal()
{
    NX_DEBUG(this, lm("Initializing analytics engine resource %1 (%2)").args(getName(), getId()));

    if (!m_sdkEngine)
        return CameraDiagnostics::InternalServerErrorResult("SDK Engine object is not set");

    NX_DEBUG(this, "Sending engine info to the Engine %1 (%2)", getName(), getId());

    auto engineInfo = nx::sdk::makePtr<nx::sdk::analytics::EngineInfo>();
    engineInfo->setId(getId().toStdString());
    engineInfo->setName(getName().toStdString());
    m_sdkEngine->setEngineInfo(engineInfo.get());

    m_handler = std::make_unique<analytics::EngineHandler>(serverModule(), getId());
    m_sdkEngine->setHandler(m_handler.get());

    if (!sendSettingsToSdkEngine())
        return CameraDiagnostics::InternalServerErrorResult("Unable to send settings to Engine");

    const auto manifest = sdk_support::manifest<nx::vms::api::analytics::EngineManifest>(
        m_sdkEngine,
        /*device*/ QnVirtualCameraResourcePtr(),
        /*engine*/ toSharedPointer(this),
        plugin().dynamicCast<nx::vms::server::resource::AnalyticsPluginResource>(),
        makeLogger());

    if (!manifest)
        return CameraDiagnostics::PluginErrorResult("Can't deserialize engine manifest");

    auto parentPlugin = plugin().dynamicCast<resource::AnalyticsPluginResource>();
    if (!parentPlugin)
        return CameraDiagnostics::PluginErrorResult("Can't find parent plugin");

    const auto pluginManifest = parentPlugin->manifest();

    nx::analytics::DescriptorManager descriptorManager(commonModule());
    descriptorManager.updateFromEngineManifest(pluginManifest.id, getId(), getName(), *manifest);

    setManifest(*manifest);
    saveProperties();

    emit engineInitialized(toSharedPointer(this));

    return CameraDiagnostics::NoErrorResult();
}

} // nx::vms::server::resource
