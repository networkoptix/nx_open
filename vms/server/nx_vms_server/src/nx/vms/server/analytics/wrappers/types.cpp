#include "types.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::server::analytics::wrappers, SdkObjectType,
    (nx::vms::server::analytics::wrappers::SdkObjectType::undefined, "undefined")
    (nx::vms::server::analytics::wrappers::SdkObjectType::plugin, "Plugin")
    (nx::vms::server::analytics::wrappers::SdkObjectType::engine, "Engine")
    (nx::vms::server::analytics::wrappers::SdkObjectType::deviceAgent, "DeviceAgent")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::server::analytics::wrappers, SdkMethod,
    (nx::vms::server::analytics::wrappers::SdkMethod::undefined, "undefined")
    (nx::vms::server::analytics::wrappers::SdkMethod::manifest, "manifest")
    (nx::vms::server::analytics::wrappers::SdkMethod::setSettings, "setSettings")
    (nx::vms::server::analytics::wrappers::SdkMethod::pluginSideSettings, "pluginSideSettings")
    (nx::vms::server::analytics::wrappers::SdkMethod::setHandler, "setHandler")
    (nx::vms::server::analytics::wrappers::SdkMethod::createEngine, "createEngine")
    (nx::vms::server::analytics::wrappers::SdkMethod::setEngineInfo, "setEngineInfo")
    (nx::vms::server::analytics::wrappers::SdkMethod::isCompatible, "isCompatible")
    (nx::vms::server::analytics::wrappers::SdkMethod::obtainDeviceAgent, "obtainDeviceAgent")
    (nx::vms::server::analytics::wrappers::SdkMethod::executeAction, "executeAction")
    (
        nx::vms::server::analytics::wrappers::SdkMethod::setNeededMetadataTypes,
        "setNeededMetadataTypes"
    )
    (nx::vms::server::analytics::wrappers::SdkMethod::pushDataPacket, "pushDataPacket")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::server::analytics::wrappers, ViolationType,
    (nx::vms::server::analytics::wrappers::ViolationType::undefined, "undefined")
    (
        nx::vms::server::analytics::wrappers::ViolationType::internalViolation,
        "internalViolation")
    (nx::vms::server::analytics::wrappers::ViolationType::nullManifest, "nullManifest")
    (
        nx::vms::server::analytics::wrappers::ViolationType::nullManifestString,
        "nullManifestString"
    )
    (
        nx::vms::server::analytics::wrappers::ViolationType::emptyManifestString,
        "emptyManifestString"
    )
    (nx::vms::server::analytics::wrappers::ViolationType::invalidJson, "invalidJson")
    (
        nx::vms::server::analytics::wrappers::ViolationType::invalidJsonStructure,
        "invalidJsonStructure"
    )
    (
        nx::vms::server::analytics::wrappers::ViolationType::manifestLogicalError,
        "manifestLogicalError"
    )
    (nx::vms::server::analytics::wrappers::ViolationType::nullEngine, "nullEngine")
    (nx::vms::server::analytics::wrappers::ViolationType::inconsistentActionResult,
        "inconsistentActionResult")
    (nx::vms::server::analytics::wrappers::ViolationType::invalidActionResultUrl,
        "invalidActionResultUrl")
)
