#include "analytics_engine_resource.h"

#include <media_server/media_server_module.h>

#include <nx/mediaserver/sdk_support/utils.h>
#include <nx/mediaserver/sdk_support/pointers.h>
#include <nx/mediaserver/analytics/sdk_object_factory.h>
#include <nx/mediaserver/interactive_settings/json_engine.h>

#include <nx/vms/api/analytics/descriptors.h>

#include <nx/sdk/analytics/plugin.h>
#include <nx/utils/meta/member_detector.h>
#include <nx/analytics/descriptor_list_manager.h>

namespace nx::mediaserver::resource {

namespace analytics_sdk = nx::sdk::analytics;
namespace analytics_api = nx::vms::api::analytics;

namespace {

using PluginPtr = sdk_support::SharedPtr<analytics_sdk::Plugin>;
using EnginePtr = sdk_support::SharedPtr<analytics_sdk::Engine>;

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
    jsonEngine.load(parentPluginManifest->engineSettingsModel);
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
    jsonEngine.load(parentPluginManifest->engineSettingsModel);
    jsonEngine.applyValues(settingsFromProperty);
    jsonEngine.applyValues(values);

    setProperty(kSettingsValuesProperty,
        QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(values)).toJson()));
}

std::optional<analytics_api::PluginManifest> AnalyticsEngineResource::pluginManifest() const
{
    auto parentPlugin = plugin().dynamicCast<resource::AnalyticsPluginResource>();
    if (!parentPlugin)
        return std::nullopt;

    return parentPlugin->manifest();
}

CameraDiagnostics::Result AnalyticsEngineResource::initInternal()
{
    NX_DEBUG(this, lm("Initializing analytics engine resource %1 (%2)")
        .args(getName(), getId()));

    if (!m_sdkEngine)
        return CameraDiagnostics::PluginErrorResult("SDK analytics engine object is not set");

    const auto manifest = sdk_support::manifest<analytics_api::EngineManifest>(m_sdkEngine);
    if (!manifest)
        return CameraDiagnostics::PluginErrorResult("Can't deserialize engine manifest");

    auto sdkSettings = sdk_support::toSdkSettings(settingsValues());
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
        sdk_support::descriptorsFromItemList<analytics_api::GroupDescriptor>(
            pluginManifest.id, manifest->groups));

    analyticsDescriptorListManager->addDescriptors(
        sdk_support::descriptorsFromItemList<analytics_api::EventTypeDescriptor>(
            pluginManifest.id, manifest->eventTypes));

    analyticsDescriptorListManager->addDescriptors(
        sdk_support::descriptorsFromItemList<analytics_api::ObjectTypeDescriptor>(
            pluginManifest.id, manifest->objectTypes));

    analyticsDescriptorListManager->addDescriptors(
        sdk_support::descriptorsFromItemList<analytics_api::ActionTypeDescriptor>(
            pluginManifest.id, manifest->objectActions));

    setManifest(*manifest);
    saveParams();
    return CameraDiagnostics::NoErrorResult();
}

} // nx::mediaserver::resource
