#pragma once

#include <QtCore/QString>

#include <core/resource/resource_fwd.h>
#include <nx/vms/server/resource/resource_fwd.h>

#include <nx/sdk/common/string_map.h>
#include <nx/sdk/analytics/i_engine.h>
#include <nx/sdk/analytics/i_device_agent.h>

#include <nx/utils/log/log.h>
#include <nx/vms/server/sdk_support/pointers.h>

namespace nx::vms::server::analytics::debug_helpers {

/** @return Null if the file does not exist, or on error. */
sdk_support::UniquePtr<nx::sdk::IStringMap> loadSettingsFromFile(
    const QString& fileDescription,
    const QString& filename);

sdk_support::UniquePtr<nx::sdk::IStringMap> loadDeviceAgentSettingsFromFile(
    const QnVirtualCameraResourcePtr& device,
    const nx::vms::server::resource::AnalyticsEngineResourcePtr& engine);

sdk_support::UniquePtr<nx::sdk::IStringMap> loadEngineSettingsFromFile(
    const nx::vms::server::resource::AnalyticsEngineResourcePtr& engine);

QString filename(
    const QnVirtualCameraResourcePtr& device,
    const nx::vms::server::resource::AnalyticsEngineResourcePtr& engine,
    const nx::vms::server::resource::AnalyticsPluginResourcePtr& plugin,
    const QString& postfix);

QString filename(const nx::sdk::analytics::IPlugin* plugin, const QString& postfix);

/** If the string is not empty and does not end with `\n`, `\n` will be added. */
void dumpStringToFile(
    const nx::utils::log::Tag& logTag,
    const QString& stringToDump,
    const QString& directoryPath,
    const QString& filename);

} // namespace nx::vms::server::analytics::debug_helpers
