#pragma once

#include <set>
#include <map>
#include <functional>

#include <core/resource/resource_fwd.h>

#include <nx/vms/server/sdk_support/error.h>
#include <nx/vms/server/resource/resource_fwd.h>

#include <nx/fusion/model_functions_fwd.h>

#include <nx/utils/log/log.h>

namespace nx::vms::server::analytics::wrappers {

struct DebugSettings
{
    QString outputPath;
    QString substitutePath;
    nx::utils::log::Tag logTag;
};

enum class SdkObjectType
{
    undefined,
    plugin,
    engine,
    deviceAgent,
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(SdkObjectType)

enum class SdkMethod
{
    undefined,

    // Common methods shared by multiple SDK interfaces.
    manifest,
    setSettings,
    pluginSideSettings,
    setHandler,

    // Plugin.
    createEngine,

    // Engine.
    setEngineInfo,
    isCompatible,
    obtainDeviceAgent,
    executeAction,

    // DeviceAgent.
    setNeededMetadataTypes,
    pushDataPacket,
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(SdkMethod);

enum class ViolationType
{
    undefined,
    internalViolation,
    nullManifest,
    nullManifestString,
    emptyManifestString,
    invalidJsonStructure,
    invalidJson,
    manifestLogicalError,

    nullEngine,

    inconsistentActionResult,
    invalidActionResultUrl,
    methodExecutionTookTooLong,
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ViolationType);

struct Violation
{
    ViolationType type = ViolationType::undefined;
    QString details;
};

using ProcessorErrorHandler = std::function<void(const sdk_support::Error&)>;

} // namespace nx::vms::server::analytics::wrappers

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (nx::vms::server::analytics::wrappers::SdkObjectType), (lexical))

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (nx::vms::server::analytics::wrappers::SdkMethod), (lexical))

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (nx::vms::server::analytics::wrappers::ViolationType), (lexical))

