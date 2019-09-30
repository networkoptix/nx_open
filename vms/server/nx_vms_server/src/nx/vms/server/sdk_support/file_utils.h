#pragma once

#include <optional>

#include <nx/utils/log/log.h>

#include <core/resource/resource_fwd.h>
#include <nx/vms/server/resource/resource_fwd.h>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::server::sdk_support {

enum class FilenameGenerationOption
{
    engineSpecific =  1 << 0,
    deviceSpecific =  1 << 1,
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(FilenameGenerationOption)

Q_DECLARE_FLAGS(FilenameGenerationOptions, FilenameGenerationOption)
Q_DECLARE_OPERATORS_FOR_FLAGS(FilenameGenerationOptions)

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
    FilenameGenerationOptions filenameGenerationOptions =
        FilenameGenerationOption::engineSpecific | FilenameGenerationOption::deviceSpecific);

} // namespace nx::vms::server::sdk_support

QN_FUSION_DECLARE_FUNCTIONS(
    nx::vms::server::sdk_support::FilenameGenerationOption, (metatype)(numeric)(lexical)(debug))
QN_FUSION_DECLARE_FUNCTIONS(
    nx::vms::server::sdk_support::FilenameGenerationOptions, (metatype)(numeric)(lexical)(debug))
