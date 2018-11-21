#include "analytics_engine_resource.h"

#include <media_server/media_server_module.h>
#include <plugins/plugins_ini.h>

#include <nx/mediaserver/sdk_support/utils.h>
#include <nx/mediaserver/sdk_support/pointers.h>
#include <nx/mediaserver/analytics/sdk_object_factory.h>
#include <nx/mediaserver/analytics/debug_helpers.h>
#include <nx/mediaserver/interactive_settings/json_engine.h>

#include <nx/vms/api/analytics/descriptors.h>

#include <nx/sdk/analytics/plugin.h>
#include <nx/utils/meta/member_detector.h>
#include <nx/analytics/descriptor_list_manager.h>

namespace nx::mediaserver::resource {

namespace {

using PluginPtr = sdk_support::SharedPtr<nx::sdk::analytics::Plugin>;
using EnginePtr = sdk_support::SharedPtr<nx::sdk::analytics::Engine>;

} // namespace

AnalyticsEngineResource::AnalyticsEngineResource(QnMediaServerModule* serverModule):
    base_type(),
    ServerModuleAware(serverModule)
{
}

void AnalyticsEngineResource::setSdkEngine(
    sdk_support::SharedPtr<nx::sdk::analytics::Engine> sdkEngine)
{
    m_sdkEngine = std::move(sdkEngine);
}

EnginePtr AnalyticsEngineResource::sdkEngine() const
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
    if (!parentPluginManifest)
    {
        NX_ERROR(
            this,
            "Unable to apply settings to analytics engine %1 (%2) "
            "since there is no correspondent plugin resource",
            getName(), getId());

        return; //< Consider making this method returning bool or some kind of error.
    }

    interactive_settings::JsonEngine jsonEngine;
    jsonEngine.loadModelFromJsonObject(parentPluginManifest->engineSettingsModel);
    jsonEngine.applyValues(settingsFromProperty);
    jsonEngine.applyValues(values);

    setProperty(kSettingsValuesProperty,
        QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(values)).toJson()));
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
        nx::utils::log::Tag(typeid(this)),
        messageTemplate,
        toSharedPointer(this));
}

CameraDiagnostics::Result AnalyticsEngineResource::initInternal()
{
    NX_DEBUG(this, lm("Initializing analytics engine resource %1 (%2)")
        .args(getName(), getId()));

    if (!m_sdkEngine)
        return CameraDiagnostics::InternalServerErrorResult("SDK Engine object is not set");

    m_handler = std::make_unique<analytics::EngineHandler>(serverModule(), toSharedPointer(this));
    m_sdkEngine->setHandler(m_handler.get());

    if (!m_sdkEngine)
        return CameraDiagnostics::PluginErrorResult("SDK analytics engine object is not set");

    const auto manifest = sdk_support::manifest<nx::vms::api::analytics::EngineManifest>(
        m_sdkEngine,
        makeLogger());

    if (!manifest)
        return CameraDiagnostics::PluginErrorResult("Can't deserialize engine manifest");

    sdk_support::UniquePtr<nx::sdk::Settings> sdkSettings;
    if (pluginsIni().analyticsEngineSettingsPath[0] != '\0')
    {
        NX_WARNING(
            this,
            "Trying to load settings for the Engine from the file. Engine %1 (%2)",
            getName(),
            getId());

        sdkSettings = analytics::debug_helpers::loadEngineSettingsFromFile(toSharedPointer(this));
    }

    if (!sdkSettings)
        sdkSettings = sdk_support::toSdkSettings(settingsValues());

    if (!sdkSettings)
    {
        NX_ERROR(this, "Unable to get settings for Engine %1 (%2)", getName(), getId());
        return CameraDiagnostics::InternalServerErrorResult("Unable to fetch settings");
    }

    m_sdkEngine->setSettings(sdkSettings.get());

    auto analyticsDescriptorListManager = sdk_support::getDescriptorListManager(serverModule());
    if (!analyticsDescriptorListManager)
    {
        return CameraDiagnostics::InternalServerErrorResult(
            "Can't access analytics descriptor list manager");
    }

    auto parentPlugin = plugin().dynamicCast<resource::AnalyticsPluginResource>();
    if (!parentPlugin)
        return CameraDiagnostics::PluginErrorResult("Can't find parent plugin");

    const auto pluginManifest = parentPlugin->manifest();

    analyticsDescriptorListManager->addDescriptors(
        sdk_support::descriptorsFromItemList<nx::vms::api::analytics::GroupDescriptor>(
            pluginManifest.id, manifest->groups));

    analyticsDescriptorListManager->addDescriptors(
        sdk_support::descriptorsFromItemList<nx::vms::api::analytics::EventTypeDescriptor>(
            pluginManifest.id, manifest->eventTypes));

    analyticsDescriptorListManager->addDescriptors(
        sdk_support::descriptorsFromItemList<nx::vms::api::analytics::ObjectTypeDescriptor>(
            pluginManifest.id, manifest->objectTypes));

    analyticsDescriptorListManager->addDescriptors(
        sdk_support::descriptorsFromItemList<nx::vms::api::analytics::ActionTypeDescriptor>(
            pluginManifest.id, manifest->objectActions));

    setManifest(*manifest);
    saveParams();
    return CameraDiagnostics::NoErrorResult();
}

} // nx::mediaserver::resource
