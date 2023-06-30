// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_data_helper.h"

#include <nx/vms/common/resource/analytics_plugin_resource.h>

namespace nx::vms::client::core {

AnalyticsEngineInfo engineInfoFromResource(
    const common::AnalyticsEngineResourcePtr& engine, SettingsModelSource settingsModelSource)
{
    const auto plugin = engine->getParentResource().dynamicCast<common::AnalyticsPluginResource>();
    if (!plugin)
        return {};

    const auto pluginManifest = plugin->manifest();

    auto settingsModel = settingsModelSource == SettingsModelSource::manifest
        ? engine->manifest().deviceAgentSettingsModel
        : QJson::deserialized<QJsonObject>(
            engine->getProperty(common::AnalyticsEngineResource::kSettingsModelProperty).toUtf8());

    return AnalyticsEngineInfo {
        engine->getId(),
        engine->getName(),
        pluginManifest.description,
        pluginManifest.version,
        pluginManifest.vendor,
        pluginManifest.isLicenseRequired,
        std::move(settingsModel),
        engine->isDeviceDependent(),
        plugin->integrationType()
    };
}

} // namespace nx::vms::client::core
