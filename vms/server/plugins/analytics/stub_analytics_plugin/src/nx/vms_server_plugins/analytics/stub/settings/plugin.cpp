// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin.h"

#include <nx/kit/debug.h>
#include <nx/kit/utils.h>

#include "engine.h"
#include "settings_model.h"

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
    return /*suppress newline*/ 1 + (const char*) R"json(
{
    "id": ")json" + instanceId() + R"json(",
    "name": "Stub: Settings",
    "description": "A plugin for testing and debugging Settings.",
    "version": "1.0.0",
    "vendor": "Plugin vendor",
    "engineSettingsModel":
    {
        "type": "Settings",
        "items":
        [
            {
                "type": "GroupBox",
                "caption": "Example Stub Engine settings",
                "items":
                [
                    {
                        "type": "TextField",
                        "name": "text",
                        "caption": "Text Field",
                        "description": "A text field",
                        "defaultValue": "a text"
                    },
                    {
                        "type": "PasswordField",
                        "name": "passwordField1",
                        "caption": "Password Field",
                        "description": "A password field",
                        "defaultValue": "1234",
                        "validationErrorMessage": "Password must contain only digits",
                        "validationRegex": "^[0-9]+$",
                        "validationRegexFlags": "i"
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
                    },
                    {
                        "type": "Link",
                        "caption": "Customer Support",
                        "url": "https://example.com/"
                    },
                    {
                        "type": "Banner",
                        "icon": "info",
                        "text": "Some text"
                    },
                    {
                        "type": "Placeholder",
                        "header": "Header",
                        "description": "Description",
                        "icon": "default"
                    }
                ]
            },
            {
                "type": "GroupBox",
                "caption": ")json" + kActiveSettingsGroupBoxCaption + R"json(",
                "items":
                [
                    {
                        "type": "ComboBox",
                        "name": ")json" + kActiveComboBoxId + R"json(",
                        "caption": "Active ComboBox",
                        "defaultValue": "Some value",
                        "isActive": true,
                        "range":
                        [
                            "Some value",
                            ")json" + kShowAdditionalComboBoxValue + R"json("
                        ]
                    },
                    {
                        "type": "CheckBox",
                        "name": ")json" + kActiveCheckBoxId + R"json(",
                        "caption": "Active CheckBox",
                        "defaultValue": false,
                        "isActive": true
                    },
                    {
                        "type": "RadioButtonGroup",
                        "name": ")json" + kActiveRadioButtonGroupId + R"json(",
                        "caption": "Active RadioButton Group",
                        "defaultValue": "Some value",
                        "isActive": true,
                        "range":
                        [
                            "Some value",
                            ")json" + kShowAdditionalRadioButtonValue + R"json("
                        ]
                    },
                    {
                        "type": "Button",
                        "name": ")json" + kShowMessageButtonId + R"json(",
                        "caption": "Show Message...",
                        "isActive": true,
                        "parametersModel": )json" + kParametersModel + R"json(
                    },
                    {
                        "type": "Button",
                        "name": ")json" + kShowUrlButtonId + R"json(",
                        "caption": "Show Webpage...",
                        "isActive": true
                    }
                ]
            }
        ]
    }
}
)json";
}

} // namespace settings
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
