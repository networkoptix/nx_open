// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace error_reporting {

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("stub_analytics_plugin_error_reporting.ini") { reload(); }

    NX_INI_FLAG(false, enableOutput, "");

    NX_INI_FLAG(false, errorOnPushCompressedFrame,
        "Produce an error on pushCompressedFrame() every 100 frames.");

    //---------------------------------------------------------------------------------------------
    // Manifests.

    NX_INI_FLAG(false, returnIncorrectIntegrationManifest,
        "Return incorrect Integration manifest.");
    NX_INI_FLAG(false, returnIncorrectEngineManifest,
        "Return incorrect Engine manifest.");
    NX_INI_FLAG(false, returnIncorrectDeviceAgentManifest,
        "Return incorrect DeviceAgent manifest.");

    //---------------------------------------------------------------------------------------------
    // Initialization.

    NX_INI_FLAG(false, returnNullOnEngineCreation,
        "Return null instead of Engine instance on initialization.");
    NX_INI_FLAG(false, returnErrorOnEngineCreation,
        "Return an error instead of Engine instance on initialization.");
    NX_INI_FLAG(false, returnErrorOnDeviceAgentCreation,
        "Return an error instead of DeviceAgent instance on initialization.");
    NX_INI_FLAG(false, returnNullOnDeviceAgentCreation,
        "Return null pointer instead of DeviceAgent instance on initialization.");

    static constexpr const char* kNoError = "noError";
    static constexpr const char* kErrorInsteadOfSettingsResponse =
        "errorInsteadOfSettingsResponse";
    static constexpr const char* kSettingsResponseWithError = "settingsResponseWithError";

    //---------------------------------------------------------------------------------------------
    // Engine settings.

    NX_INI_STRING(kNoError, returnErrorFromEngineSetSettings,
        "Return an Error from Engine on setSettings().\n"
        "String value \"noError\": No error.\n"
        "String value \"errorInsteadOfSettingsResponse\": Return an error instead of Settings "
        "Response from the Engine on setting Settings.\n"
        "String value \"settingsResponseWithError\": Return Settings Response containing an error "
        "from the Engine on setting Settings.");


    NX_INI_STRING(kNoError, returnErrorFromEngineActiveSettingChange,
        "Return an Error from Engine on setting Active Settings.\n"
        "String value \"noError\": No error.\n"
        "String value \"errorInsteadOfSettingsResponse\": Return an error instead of Settings "
        "Response from Engine on setting Active Settings.\n"
        "String value \"settingsResponseWithError\": Return Settings Response containing an error "
        "from Engine on setting Active Settings.");


    NX_INI_STRING(kNoError, returnErrorFromEngineOnGetIntegrationSideSettings,
        "Return an Error from Engine on setting Integration-side Settings.\n"
        "String value \"noError\": No error.\n"
        "String value \"errorInsteadOfSettingsResponse\": Return an error instead of Settings "
        "Response from Engine's Integration-side Settings.\n"
        "String value \"settingsResponseWithError\": Return Settings Response containing an error "
        "from Engine's Integration-side Settings.");

    //---------------------------------------------------------------------------------------------
    // DeviceAgent settings.

    NX_INI_STRING(kNoError, returnErrorFromDeviceAgentSetSettings,
        "Return an Error from DeviceAgent on setting Settings.\n"
        "String value \"noError\": No error.\n"
        "String value \"errorInsteadOfSettingsResponse\": Return an error instead of Settings "
        "Response from DeviceAgent on setting Settings.\n"
        "String value \"settingsResponseWithError\": Return Settings Response containing an error "
        "from DeviceAgent on setting Settings.");


    NX_INI_STRING(kNoError, returnErrorFromDeviceAgentActiveSettingChange,
        "Return an Error from DeviceAgent on setting Active Settings.\n"
        "String value \"noError\": No error.\n"
        "String value \"errorInsteadOfSettingsResponse\": Return an error instead of Settings "
        "Response from DeviceAgent on setting Active Settings."
        "String value \"settingsResponseWithError\": Return Settings Response containing an error "
        "from DeviceAgent on setting Active Settings.");

    NX_INI_STRING(kNoError, returnErrorFromDeviceAgentOnGetIntegrationSideSettings,
        "Return an Error from DeviceAgent on setting Integration-side Settings.\n"
        "String value \"noError\": No error.\n"
        "String value \"errorInsteadOfSettingsResponse\": Return an error instead of Settings "
        "Response from DeviceAgent's Integration-side Settings.\n"
        "String value \"settingsResponseWithError\": Return Settings Response containing an error "
        "from DeviceAgent's Integration-side Settings.");
};

Ini& ini();

} // namespace error_reporting
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
