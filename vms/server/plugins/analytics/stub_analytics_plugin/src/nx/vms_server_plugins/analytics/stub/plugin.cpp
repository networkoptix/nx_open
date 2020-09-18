// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/kit/debug.h>
#include <nx/kit/utils.h>

#include "plugin.h"
#include "engine.h"
#include "settings_model.h"
#include "stub_analytics_plugin_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Result<IEngine*> Plugin::doObtainEngine()
{
    return new Engine(this);
}

std::string Plugin::manifestString() const
{
    const std::string instanceIdSuffixForMultiPluginMode =
        (ini().multiPluginInstanceCount <= 0)
            ? ""
            : nx::kit::utils::format(".%d", m_instanceIndexForMultiPluginMode);

    const std::string instanceNameSuffixForMultiPluginMode =
        (ini().multiPluginInstanceCount <= 0)
            ? ""
            : nx::kit::utils::format(" #%d", m_instanceIndexForMultiPluginMode);

    return /*suppress newline*/ 1 + (const char*) R"json(
{
    "id": "nx.stub)json" + instanceIdSuffixForMultiPluginMode + R"json(",
    "name": "Stub analytics plugin)json" + instanceNameSuffixForMultiPluginMode + R"json(",
    "description": "A plugin for testing and debugging purposes.",
    "version": "1.0.0",
    "vendor": "Plugin vendor",
    "engineSettingsModel": {
        "type": "Settings",
        "items": [
            {
                "type": "GroupBox",
                "caption": "Real Stub Engine settings",
                "items": [
                    {
                        "type": "CheckBox",
                        "name": ")json" + kThrowPluginDiagnosticEventsFromEngineSetting + R"json(",
                        "caption": "Produce Plugin Diagnostic Events from the Engine",
                        "defaultValue": false
                    },
                    {
                        "type": "CheckBox",
                        "name": ")json" + kDisableStreamSelectionSetting + R"json(",
                        "caption": "Disable stream selection",
                        "description": "If true, the stream selection control will be hidden for newly created DeviceAgents",
                        "defaultValue": false
                    }
                ]
            },
            {
                "type": "GroupBox",
                "caption": "Example Stub Engine settings",
                "items": [
                    {
                        "type": "TextField",
                        "name": "text",
                        "caption": "Text Field",
                        "description": "A text field",
                        "defaultValue": "a text"
                    },
                    {
                        "type": "SpinBox",
                        "name": "testSpinBox",
                        "caption": "Spin Box",
                        "defaultValue": 42,
                        "minValue": 0,
                        "maxValue": 100
                    },
                    {
                        "type": "SpinBox",
                        "name": ")json" + kEnginePluginSideSetting + R"json(",
                        "caption": "Spin Box (plugin side)",
                        "defaultValue": 42,
                        "minValue": 0,
                        "maxValue": 100
                    },
                    {
                        "type": "DoubleSpinBox",
                        "name": "testDoubleSpinBox",
                        "caption": "Double Spin Box",
                        "defaultValue": 3.1415,
                        "minValue": 0.0,
                        "maxValue": 100.0
                    },
                    {
                        "type": "ComboBox",
                        "name": "testComboBox",
                        "caption": "Combo Box",
                        "defaultValue": "value2",
                        "range": ["value1", "value2", "value3"]
                    },
                    {
                        "type": "CheckBox",
                        "name": "testCheckBox",
                        "caption": "Check Box",
                        "defaultValue": true
                    }
                ]
            }
        ]
    }
}
)json";
}

extern "C" NX_PLUGIN_API nx::sdk::IPlugin* createNxPlugin()
{
    if (ini().multiPluginInstanceCount <= 0)
        return new Plugin();

    return nullptr;
}

extern "C" NX_PLUGIN_API nx::sdk::IPlugin* createNxPluginByIndex(int instanceIndex)
{
    if (ini().multiPluginInstanceCount <= 0)
        return nullptr;

    if (!NX_KIT_ASSERT(instanceIndex >= 0))
        return nullptr;

    if (instanceIndex >= ini().multiPluginInstanceCount)
        return nullptr;

    return new Plugin(instanceIndex);
}

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
