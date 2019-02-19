#pragma once

#include <QtCore/QString>

#include <core/resource/resource_fwd.h>
#include <nx/vms/server/resource/resource_fwd.h>

#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/analytics/i_engine.h>
#include <nx/sdk/analytics/i_device_agent.h>

#include <nx/utils/log/log.h>
#include <nx/sdk/helpers/ptr.h>

namespace nx::vms::server::analytics::debug_helpers {

/** @return Null if the file does not exist, or on error. */
nx::sdk::Ptr<nx::sdk::IStringMap> loadSettingsFromFile(
    const QString& fileDescription,
    const QString& filename);

nx::sdk::Ptr<nx::sdk::IStringMap> loadDeviceAgentSettingsFromFile(
    const QnVirtualCameraResourcePtr& device,
    const nx::vms::server::resource::AnalyticsEngineResourcePtr& engine);

nx::sdk::Ptr<nx::sdk::IStringMap> loadEngineSettingsFromFile(
    const nx::vms::server::resource::AnalyticsEngineResourcePtr& engine);

QString nameOfFileToDumpDataTo(
    const QnVirtualCameraResourcePtr& device,
    const nx::vms::server::resource::AnalyticsEngineResourcePtr& engine,
    const nx::vms::server::resource::AnalyticsPluginResourcePtr& plugin,
    const QString& postfix);

QString nameOfFileToDumpDataTo(const nx::sdk::analytics::IPlugin* plugin, const QString& postfix);

/** If the string is not empty and does not end with `\n`, `\n` will be added. */
void dumpStringToFile(
    const nx::utils::log::Tag& logTag,
    const QString& stringToDump,
    const QString& directoryPath,
    const QString& filename);

} // namespace nx::vms::server::analytics::debug_helpers
