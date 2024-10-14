// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/kit/debug.h>
#include <nx/kit/utils.h>
#include <nx/sdk/helpers/error.h>

#include "integration.h"
#include "engine.h"
#include "stub_analytics_plugin_error_reporting_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace error_reporting {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Result<IEngine*> Integration::doObtainEngine()
{
    if (ini().returnNullOnEngineCreation)
    {
        return nullptr;
    }
    else if (ini().returnErrorOnEngineCreation)
    {
        return error(ErrorCode::internalError, "Unable to create Engine instance.");
    }

    return new Engine(this);
}

std::string Integration::manifestString() const
{
    if (ini().returnIncorrectIntegrationManifest)
        return "incorrectIntegrationManifest";

    return /*suppress newline*/ 1 + (const char*) R"json(
{
    "id": ")json" + instanceId() + R"json(",
    "name": "Stub, Error Reporting",
    "description": "A plugin for error reporting.",
    "version": "1.0.0",
    "vendor": "Plugin vendor",
    "engineSettingsModel":
    {
        "type": "Settings",
        "items":
        [
            {
                "type": "CheckBox",
                "name": "Test Engine's setSettings",
                "caption": "Test Engine's setSettings()",
                "defaultValue": false
            },
            {
                "type": "CheckBox",
                "name": "Test Engine's Active Settings",
                "caption": "Test Engine's Active Settings",
                "defaultValue": false,
                "isActive": true
            },
            {
                "type": "TextField",
                "name": "Engine SetSettings Error Key",
                "caption": "",
                "defaultValue": "",
                "visible": false
            },
            {
                "type": "TextField",
                "name": "Engine GetSettingsOnActiveSettingChange Error Key",
                "caption": "",
                "defaultValue": "",
                "visible": false
            },
            {
                "type": "TextField",
                "name": "Engine getIntegrationSideSettings Error Key",
                "caption": "",
                "defaultValue": "",
                "visible": false
            }
        ]
    }
}
)json";
}

} // namespace error_reporting
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
