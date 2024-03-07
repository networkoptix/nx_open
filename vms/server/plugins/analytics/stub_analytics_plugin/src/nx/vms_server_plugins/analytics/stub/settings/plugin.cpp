// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin.h"

#include <nx/kit/debug.h>
#include <nx/kit/utils.h>

#include "engine.h"
#include "settings_model.h"

#include <nx/vms_server_plugins/analytics/stub/settings/stub_analytics_plugin_settings_ini.h>
#include <nx/vms_server_plugins/analytics/stub/utils.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace settings {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Result<IEngine*> Plugin::doObtainEngine()
{
    return new Engine(this);
}

std::string Plugin::manifestString() const
{
    std::string engineSettingsModel = kEngineSettingsModel;

    if (ini().showExtraCheckBox)
    {
        engineSettingsModel = substituteAllTemplateVariables(engineSettingsModel,
            {{kExtraCheckBoxTemplateVariableName, kExtraCheckBoxJson}});
    }
    else
    {
        engineSettingsModel = substituteAllTemplateVariables(engineSettingsModel, {});
    }

    return /*suppress newline*/ 1 + (const char*) R"json(
{
    "id": ")json" + instanceId() + R"json(",
    "name": "Stub, Settings",
    "description": "A plugin for testing and debugging Settings.",
    "version": "1.0.0",
    "vendor": "Plugin vendor",
    "engineSettingsModel": )json" + engineSettingsModel + R"json(
}
)json";
}

} // namespace settings
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
