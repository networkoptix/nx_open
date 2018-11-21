#pragma once

#include <QtCore/QString>

#include <core/resource/resource_fwd.h>
#include <nx/mediaserver/resource/resource_fwd.h>

#include <nx/sdk/common_settings.h>
#include <nx/sdk/analytics/engine.h>
#include <nx/sdk/analytics/device_agent.h>

#include <nx/utils/log/log.h>
#include <nx/mediaserver/sdk_support/pointers.h>

namespace nx::mediaserver::analytics::debug_helpers {

/** @return nullptr if the file does not exist, or on error. */
sdk_support::UniquePtr<nx::sdk::Settings> loadSettingsFromFile(
    const QString& fileDescription,
    const QString& filename);

sdk_support::UniquePtr<nx::sdk::Settings> loadDeviceAgentSettingsFromFile(
    const QnVirtualCameraResourcePtr& device,
    const nx::mediaserver::resource::AnalyticsEngineResourcePtr& engine);

sdk_support::UniquePtr<nx::sdk::Settings> loadEngineSettingsFromFile(
    const nx::mediaserver::resource::AnalyticsEngineResourcePtr& engine);

QString filename(
    const QnVirtualCameraResourcePtr& device,
    const nx::mediaserver::resource::AnalyticsEngineResourcePtr& engine,
    const nx::mediaserver::resource::AnalyticsPluginResourcePtr& plugin,
    const QString& postfix);

QString filename(const nx::sdk::analytics::Plugin* plugin, const QString& postfix);

void saveManifestToFile(
    const nx::utils::log::Tag& logTag,
    const QString& manifest,
    const QString& directoryPath,
    const QString& filename);

} // namespace nx::mediaserver::analytics::debug_helpers
