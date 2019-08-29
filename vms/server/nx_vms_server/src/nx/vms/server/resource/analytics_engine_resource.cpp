#include "analytics_engine_resource.h"

#include <media_server/media_server_module.h>
#include <plugins/vms_server_plugins_ini.h>

#include <nx/vms/server/sdk_support/utils.h>
#include <nx/vms/server/sdk_support/to_string.h>
#include <nx/vms/server/sdk_support/result_holder.h>
#include <nx/vms/server/interactive_settings/json_engine.h>
#include <nx/vms/server/analytics/wrappers/engine.h>
#include <nx/vms/api/analytics/descriptors.h>

#include <nx/sdk/analytics/i_plugin.h>
#include <nx/sdk/analytics/helpers/engine_info.h>
#include <nx/sdk/helpers/to_string.h>
#include <nx/sdk/i_string_map.h>
#include <nx/utils/member_detector.h>
#include <nx/analytics/descriptor_manager.h>

namespace nx::vms::server::resource {

using namespace nx::vms::server::analytics;

template<typename T>
using ResultHolder = nx::vms::server::sdk_support::ResultHolder<T>;

AnalyticsEngineResource::AnalyticsEngineResource(QnMediaServerModule* serverModule):
    base_type(),
    ServerModuleAware(serverModule)
{
}

void AnalyticsEngineResource::setSdkEngine(wrappers::EnginePtr sdkEngine)
{
    m_sdkEngine = std::move(sdkEngine);
}

wrappers::EnginePtr AnalyticsEngineResource::sdkEngine() const
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

    std::optional<QVariantMap> effectiveSettings;
    if (pluginsIni().analyticsSettingsSubstitutePath[0] != '\0')
    {
        NX_WARNING(this, "Trying to load settings for the Engine %1 (%2) from a file as per %3",
            getName(), getId(), pluginsIni().iniFile());

        effectiveSettings = loadSettingsFromFile();
    }

    if (!effectiveSettings)
        effectiveSettings = settingsValues();

    if (!NX_ASSERT(effectiveSettings, lm("Engine %1 (%2)").args(getName(), getId())))
        return false;

    return engine->setSettings(*effectiveSettings).has_value();
}

std::optional<nx::vms::api::analytics::PluginManifest>
    AnalyticsEngineResource::pluginManifest() const
{
    auto parentPlugin = plugin().dynamicCast<resource::AnalyticsPluginResource>();
    if (!parentPlugin)
        return std::nullopt;

    return parentPlugin->manifest();
}

CameraDiagnostics::Result AnalyticsEngineResource::initInternal()
{
    NX_DEBUG(this, lm("Initializing Analytics Engine resource %1 (%2)").args(getName(), getId()));

    if (!m_sdkEngine)
        return CameraDiagnostics::InternalServerErrorResult("SDK Engine object is not set");

    NX_DEBUG(this, "Sending engine info to the Engine %1 (%2)", getName(), getId());

    auto engineInfo = nx::sdk::makePtr<nx::sdk::analytics::EngineInfo>();
    engineInfo->setId(getId().toStdString());
    engineInfo->setName(getName().toStdString());
    m_sdkEngine->setEngineInfo(engineInfo);

    m_handler = nx::sdk::makePtr<analytics::EngineHandler>(serverModule(), getId());
    m_sdkEngine->setHandler(m_handler);

    if (!sendSettingsToSdkEngine())
        return CameraDiagnostics::InternalServerErrorResult("Unable to send settings to Engine");

    const auto manifest = m_sdkEngine->manifest();
    if (!manifest)
        return CameraDiagnostics::PluginErrorResult("Can't obtain Engine Manifest");

    auto parentPlugin = plugin().dynamicCast<resource::AnalyticsPluginResource>();
    if (!parentPlugin)
        return CameraDiagnostics::PluginErrorResult("Can't find parent Plugin");

    const auto pluginManifest = parentPlugin->manifest();

    nx::analytics::DescriptorManager descriptorManager(commonModule());
    descriptorManager.updateFromEngineManifest(pluginManifest.id, getId(), getName(), *manifest);

    setManifest(*manifest);
    saveProperties();

    emit engineInitialized(toSharedPointer(this));

    return CameraDiagnostics::NoErrorResult();
}

QString AnalyticsEngineResource::libName() const
{
    if (!NX_ASSERT(m_sdkEngine))
        return QString();

    return m_sdkEngine->libName();
}

std::optional<QVariantMap> AnalyticsEngineResource::loadSettingsFromFile() const
{
    const auto loadSettings =
        [this](sdk_support::FilenameGenerationOptions filenameGenerationOptions)
            -> std::optional<QVariantMap>
        {
            const QString settingsFilename = sdk_support::debugFileAbsolutePath(
                pluginsIni().analyticsSettingsSubstitutePath,
                sdk_support::baseNameOfFileToDumpOrLoadData(
                    plugin().dynamicCast<resource::AnalyticsPluginResource>(),
                    toSharedPointer(this),
                    QnVirtualCameraResourcePtr(),
                    filenameGenerationOptions)) + "_settings.json";

            std::optional<QString> settingsString = sdk_support::loadStringFromFile(
                nx::utils::log::Tag(typeid(this)),
                settingsFilename);

            if (!settingsString)
                return std::nullopt;

            return sdk_support::toQVariantMap(*settingsString);
        };

    const std::optional<QVariantMap> result = loadSettings(
        sdk_support::FilenameGenerationOption::engineSpecific);
    if (result)
        return result;

    return loadSettings(sdk_support::FilenameGenerationOptions());
}

} // nx::vms::server::resource
