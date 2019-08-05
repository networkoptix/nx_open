#pragma once

#include <optional>

#include <nx/utils/log/log.h>

#include <core/resource/resource_fwd.h>
#include <nx/vms/server/resource/resource_fwd.h>

namespace nx::vms::server::sdk_support {

std::optional<QString> loadStringFromFile(
    const nx::utils::log::Tag& logTag,
    const QString& absoluteFilePath);

bool dumpStringToFile(
    const nx::utils::log::Tag& logTag,
    const QString& absoluteFilePath,
    const QString& stringToDump);

QString debugFileAbsolutePath(
    const QString& debugDirectoryPath,
    const QString filenameWithoutPath);

QString baseNameOfFileToDumpOrLoadData(
    const resource::AnalyticsPluginResourcePtr& plugin,
    const resource::AnalyticsEngineResourcePtr& engine,
    const QnVirtualCameraResourcePtr& device,
    bool isEngineSpecific = true,
    bool isDeviceSpecific = true);

} // namespace nx::vms::server::sdk_support
