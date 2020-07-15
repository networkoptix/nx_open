// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin.h"
#include "engine.h"
#include "settings_model.h"

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
    return /*suppress newline*/ 1 + (const char*) R"json(
{
    "id": "nx.stub",
    "name": "Stub analytics plugin",
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
    return new Plugin();
}

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
