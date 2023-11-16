// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace settings {

static const std::string kRegularSettingsModelOption = "regular";
static const std::string kAlternativeSettingsModelOption = "alternative";

static const std::string kSettingsModelSettings = "settingsModelComboBox";

static const std::string kCitySelector = "languageSelectorSettings";
static const std::string kEnglishOption = "English";
static const std::string kGermanOption = "German";

// ------------------------------------------------------------------------------------------------

static const std::string kCaption = "caption";
static const std::string kSections = "sections";
static const std::string kName = "name";
static const std::string kRange = "range";
static const std::string kItems = "items";
static const std::string kIsActive = "isActive";
static const std::string kMinValue = "minValue";
static const std::string kMaxValue = "maxValue";
static const std::string kActiveSettingsSectionCaption = "Active settings section";
static const std::string kActiveSettingsGroupBoxCaption = "Active settings";

static const std::string kActiveComboBoxId = "activeComboBox";
static const std::string kAdditionalComboBoxId = "additionalComboBox";
static const std::string kShowAdditionalComboBoxValue = "Show additional ComboBox";
static const std::string kAdditionalComboBoxValue = "Value 1";

static const std::string kActiveCheckBoxId = "activeCheckBox";
static const std::string kAdditionalCheckBoxId = "additionalCheckBox";
static const std::string kShowAdditionalCheckBoxValue = "true";
static const std::string kAdditionalCheckBoxValue = "false";

static const std::string kActiveRadioButtonGroupId = "activeRadioButtonGroup";
static const std::string kShowAdditionalRadioButtonValue = "Show something";
static const std::string kHideAdditionalRadioButtonValue = "Hide me";
static const std::string kDefaultActiveRadioButtonGroupValue = "Some value";

static const std::string kActiveMinValueId = "activeMinValue";
static const std::string kActiveMaxValueId = "activeMaxValue";

static const std::string kShowMessageButtonId = "showMessageButton";
static const std::string kShowUrlButtonId = "showUrlButton";
static const std::string kUrlId = "url";
static const std::string kUseProxyId = "useProxy";
static const std::string kUseDeviceCredentialsId = "authDevice";

static const std::string kActiveCheckBoxForValueSetChangeId = "activeCheckBoxForValueSetChange";
static const std::string kComboBoxForValueSetChangeId = "comboBoxForValueSetChange";
static const std::string kComboBoxForValueSetChangeValuePermanent = "Permanent";
static const std::string kComboBoxForValueSetChangeValueOptional = "Optional";

static const std::string kParametersModel = /*suppress newline*/ 1 + R"json(
{
    "type": "Settings",
    "items":
    [
        {
            "type": "TextField",
            "name": "parameter",
            "caption": "Parameter"
        }
    ]
}
)json";

// ------------------------------------------------------------------------------------------------
static const std::string kEnginePluginSideSetting = "testPluginSideSpinBox";
static const std::string kEnginePluginSideSettingValue = "42";
static const std::string kEngineSettingsModel = /*suppress newline*/ 1 + R"json("
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
                    "type": "GroupBox",
                    "caption": "Styled GroupBox",
                    "style": "label",
                    "items": [
                        {
                            "type": "Link",
                            "caption": "Customer Support",
                            "url": "https://example.com/"
                        }
                    ]
                },
                {
                    "type": "GroupBox",
                    "caption": "Nested GroupBox",
                    "items": [
                        {
                            "type": "Banner",
                            "icon": "info",
                            "text": "Some info text"
                        },
                        {
                            "type": "Banner",
                            "icon": "warning",
                            "text": "Some warning text"
                        }
                    ]
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
                    "type": "SpinBox",
                    "name": ")json" + kActiveMinValueId + R"json(",
                    "caption": "Active Minimum",
                    "defaultValue": 42,
                    "minValue": 0,
                    "maxValue": 42,
                    "isActive": true
                },
                {
                    "type": "SpinBox",
                    "name": ")json" + kActiveMaxValueId + R"json(",
                    "caption": "Active Maximum",
                    "defaultValue": 42,
                    "minValue": 42,
                    "maxValue": 100,
                    "isActive": true
                },
                {
                    "type": "Flow",
                    "caption": "Parameters",
                    "items":
                    [
                        {
                            "type": "Button",
                            "name": ")json" + kShowMessageButtonId + R"json(",
                            "caption": "Show Message...",
                            "isActive": true,
                            "parametersModel": )json" + kParametersModel + R"json(
                        }
                    ]
                },
                {
                    "type": "Flow",
                    "caption": "Webpage",
                    "items":
                    [
                        {
                            "type": "TextField",
                            "name": ")json" + kUrlId + R"json(",
                            "caption": "Url",
                            "defaultValue": "https://example.org/"
                        },
                        {
                            "type": "Button",
                            "name": ")json" + kShowUrlButtonId + R"json(",
                            "caption": "Show Webpage...",
                            "isActive": true
                        },
                        {
                            "type": "CheckBox",
                            "name": ")json" + kUseProxyId + R"json(",
                            "caption": "Use proxy",
                            "defaultValue": false
                        }
                    ]
                }
            ]
        }
    ]
}
)json";

static const std::string kAlternativeSettingsModel =
    /*suppress newline*/ 1 + (const char*) R"json("
{
    "type": "Settings",
    "items":
    [
        {
            "type": "ComboBox",
            "name": ")json" + kSettingsModelSettings + R"json(",
            "caption": "Settings model",
            "defaultValue": ")json" + kRegularSettingsModelOption + R"json(",
            "range":
            [
                ")json" + kRegularSettingsModelOption + R"json(",
                ")json" + kAlternativeSettingsModelOption + R"json("
            ]
        },
        {
            "type": "GroupBox",
            "caption": "Alternative GroupBox",
            "items":
            [
                {
                    "type": "CheckBox",
                    "name": "alternativeCheckBox",
                    "caption": "Alternative CheckBox",
                    "defaultValue": true
                },
                {
                    "type": "TextField",
                    "name": "alternativeTextField",
                    "caption": "Alternative TextField",
                    "defaultValue": "alternative text"
                }
            ]
        }
    ]
}
)json";

static const std::string kRegularSettingsModelPart1 = /*suppress newline*/ 1 + R"json(
{
    "type": "Settings",
    "items":
    [
        {
            "type": "ComboBox",
            "name": ")json" + kSettingsModelSettings + R"json(",
            "caption": "Settings model",
            "defaultValue": ")json" + kRegularSettingsModelOption + R"json(",
            "range":
            [
                ")json" + kRegularSettingsModelOption + R"json(",
                ")json" + kAlternativeSettingsModelOption + R"json("
            ]
        },
        {
            "type": "GroupBox",
            "caption": "Example Stub DeviceAgent settings",
            "items":
            [
                {
                    "type": "TextField",
                    "name": "testTextField",
                    "caption": "Device Agent Text Field",
                    "description": "A text field",
                    "defaultValue": "a text"
                },
                {
                    "type": "ComboBox",
                    "name": ")json" + kCitySelector + R"json(",
                    "caption": "Cities",
                    "defaultValue": "English",
                    "range": [
                        ")json" + kEnglishOption + R"json(",
                        ")json" + kGermanOption + R"json("
                    ]
                },
)json";

static const std::string kEnglishCitiesSettingsModelPart = /*suppress newline*/ 1 + R"json(
                {
                    "type": "RadioButtonGroup",
                    "name": "testEnglishRadioButtonGroup",
                    "caption": "Choose one",
                    "description": "Choose one option",
                    "defaultValue": "London",
                    "range":
                    [
                        "London",
                        "Liverpool"
                    ]
                },
)json";

static const std::string kGermanCitiesSettingsModelPart = /*suppress newline*/ 1 + R"json(
                {
                    "type": "RadioButtonGroup",
                    "name": "testGermanRadioButtonGroup",
                    "caption": "Choose one",
                    "description": "Choose one option",
                    "defaultValue": "Berlin",
                    "range":
                    [
                        "Berlin",
                        "Nuremberg",
                        "Leipzig"
                    ]
                },
)json";

static const std::string kRegularSettingsModelPart2 = /*suppress newline*/ 1 + R"json("
                {
                    "type": "RadioButtonGroup",
                    "name": "testRadioButtonGroup",
                    "caption": "RadioButton Group",
                    "description": "Choose one option",
                    "defaultValue": "Cs_enodatum",
                    "range":
                    [
                        "K_gowerianus",
                        "K_galilaeii",
                        "S_calloviense",
                        "S_micans",
                        "Cs_enodatum",
                        "K_medea",
                        "K_jason",
                        "K_obductum",
                        "K_posterior"
                    ],
                    "itemCaptions":
                    {
                        "K_gowerianus": "Kepplerites gowerianus",
                        "K_galilaeii": "Kepplerites galilaeii",
                        "S_calloviense": "Sigaloceras calloviense",
                        "S_micans": "Sigaloceras micans",
                        "Cs_enodatum": "Catasigaloceras enodatum",
                        "K_medea": "Kosmoceras medea",
                        "K_jason": "Kosmoceras jason",
                        "K_obductum": "Kosmoceras obductum",
                        "K_posterior": "Kosmoceras posterior"
                    }
                },
                {
                    "type": "CheckBoxGroup",
                    "name": "testCheckBoxGroup",
                    "caption": "CheckBox Group",
                    "description": "Choose one or several options",
                    "defaultValue": ["Coleoidea", "Nautiloidea"],
                    "range":
                    [
                        "Coleoidea",
                        "Ammonoidea",
                        "Nautiloidea",
                        "Orthoceratoidea"
                    ],
                    "itemCaptions":
                    {
                        "Coleoidea": "Coleoidea (Bather, 1888)",
                        "Ammonoidea": "Ammonoidea (Zittel, 1884)",
                        "Nautiloidea": "Nautiloidea (Agassiz, 1847)",
                        "Orthoceratoidea": "Orthoceratoidea (M'Coy 1844)"
                    }
                },
                {
                    "type": "SpinBox",
                    "caption": "Device Agent SpinBox (plugin side)",
                    "name": "pluginSideTestSpinBox",
                    "defaultValue": 42,
                    "minValue": 0,
                    "maxValue": 100
                },
                {
                    "type": "DoubleSpinBox",
                    "caption": "Device Agent DoubleSpinBox",
                    "name": "testDoubleSpinBox",
                    "defaultValue": 3.1415,
                    "minValue": 0.0,
                    "maxValue": 100.0
                },
                {
                    "type": "ComboBox",
                    "name": "testComboBox",
                    "caption": "Device Agent ComboBox",
                    "defaultValue": "value2",
                    "range": ["value1", "value2", "value3"],
                    "itemCaptions":
                    {
                        "value1": "Device Agent Value #1",
                        "value2": "Device Agent Value #2",
                        "value3": "Device Agent Value #3"
                    }
                },
                {
                    "type": "Separator"
                },
                {
                    "type": "CheckBox",
                    "caption": "Device Agent CheckBox",
                    "name": "testCheckBox",
                    "defaultValue": true
                },
                {
                    "type": "CheckBox",
                    "caption": "Disabled Device Agent CheckBox",
                    "name": "disabledTestCheckBox",
                    "defaultValue": false,
                    "enabled": false
                },
                {
                    "type": "CheckBox",
                    "caption": "Hidden Device Agent CheckBox",
                    "name": "hiddenTestCheckBox",
                    "defaultValue": false,
                    "visible": false
                },
                {
                    "type": "GroupBox",
                    "caption": "Styled GroupBox",
                    "style": "label",
                    "items": [
                        {
                            "type": "Link",
                            "caption": "Customer Support",
                            "url": "https://example.com/"
                        }
                    ]
                },
                {
                    "type": "GroupBox",
                    "caption": "Nested GroupBox",
                    "items": [
                        {
                            "type": "Banner",
                            "icon": "info",
                            "text": "Some info text"
                        },
                        {
                            "type": "Banner",
                            "icon": "warning",
                            "text": "Some warning text"
                        }
                    ]
                },
                {
                    "type": "Placeholder",
                    "header": "Header",
                    "description": "Description",
                    "icon": "default"
                }
            ]
        }
    ],
    "sections":
    [
        {
            "type": "Section",
            "caption": "Example section",
            "items":
            [
                {
                    "type": "GroupBox",
                    "caption": "Example Stub DeviceAgent settings section",
                    "items":
                    [
                        {
                            "type": "TextField",
                            "name": "testTextFieldWithValidation",
                            "caption": "Hexadecimal number text field",
                            "defaultValue": "12ab34cd",
                            "validationRegex": "^[a-f0-9]+$",
                            "validationRegexFlags": "i",
                            "validationErrorMessage":
                                "Text must contain only digits and characters a-f, e.g. 12ab34cd."
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
                            "caption": "Device Agent SpinBox (plugin side)",
                            "name": "pluginSideTestSpinBox2",
                            "defaultValue": 42,
                            "minValue": 0,
                            "maxValue": 100
                        },
                        {
                            "type": "DoubleSpinBox",
                            "caption": "Device Agent DoubleSpinBox",
                            "name": "testDoubleSpinBox2",
                            "defaultValue": 3.1415,
                            "minValue": 0.0,
                            "maxValue": 100.0
                        },
                        {
                            "type": "ComboBox",
                            "name": "testComboBox2",
                            "caption": "Device Agent ComboBox",
                            "defaultValue": "value2",
                            "range": ["value1", "value2", "value3"]
                        },
                        {
                            "type": "CheckBox",
                            "caption": "Device Agent CheckBox",
                            "name": "testCheckBox2",
                            "defaultValue": true
                        }
                    ]
                }
            ],
            "sections":
            [
                {
                    "type": "Section",
                    "caption": "Nested section",
                    "items":
                    [
                        {
                            "type": "GroupBox",
                            "caption": "Example Stub DeviceAgent settings nested section",
                            "items":
                            [
                                {
                                    "type": "SwitchButton",
                                    "caption": "Switch Button",
                                    "name": "testSwitch",
                                    "description": "Tooltip for the switch button",
                                    "defaultValue": false
                                },
                                {
                                    "type": "SpinBox",
                                    "caption": "SpinBox Parameter",
                                    "name": "testSpinBox3",
                                    "defaultValue": 42,
                                    "minValue": 0,
                                    "maxValue": 100
                                },
                                {
                                    "type": "DoubleSpinBox",
                                    "caption": "DoubleSpinBox Parameter",
                                    "name": "testDoubleSpinBox3",
                                    "defaultValue": 3.1415,
                                    "minValue": 0.0,
                                    "maxValue": 100.0
                                },
                                {
                                    "type": "ComboBox",
                                    "name": "testComboBox3",
                                    "caption": "ComboBox Parameter",
                                    "defaultValue": "value2",
                                    "range": ["value1", "value2", "value3"]
                                },
                                {
                                    "type": "CheckBox",
                                    "caption": "CheckBox Parameter",
                                    "name": "testCheckBox3",
                                    "defaultValue": true
                                }
                            ]
                        }
                    ]
                }
            ]
        },
        {
            "type": "Section",
            "caption": ")json" + kActiveSettingsSectionCaption + R"json(",
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
                    "isActive": true,
                    "parametersModel":
                    {
                        "type": "Settings",
                        "items":
                        [
                            {
                                "type": "TextField",
                                "name": "testTextField",
                                "caption": "Text Field Parameter",
                                "description": "A text field",
                                "defaultValue": "a text"
                            },
                            {
                                "type": "GroupBox",
                                "caption": "Parameter Group",
                                "items":
                                [
                                    {
                                        "type": "SpinBox",
                                        "caption": "SpinBox Parameter",
                                        "name": "testSpinBox",
                                        "defaultValue": 42,
                                        "minValue": 0,
                                        "maxValue": 100
                                    },
                                    {
                                        "type": "DoubleSpinBox",
                                        "caption": "DoubleSpinBox Parameter",
                                        "name": "testDoubleSpinBox",
                                        "defaultValue": 3.1415,
                                        "minValue": 0.0,
                                        "maxValue": 100.0
                                    },
                                    {
                                        "type": "ComboBox",
                                        "caption": "ComboBox Parameter",
                                        "name": "testComboBox",
                                        "defaultValue": "value2",
                                        "range": ["value1", "value2", "value3"]
                                    },
                                    {
                                        "type": "CheckBox",
                                        "caption": "CheckBox Parameter",
                                        "name": "testCheckBox",
                                        "defaultValue": true
                                    },
                                    {
                                        "type": "TextArea",
                                        "caption": "TextArea Parameter",
                                        "name": "testTextArea"
                                    }
                                ]
                            }
                        ]
                    }
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
                    "type": "SpinBox",
                    "name": ")json" + kActiveMinValueId + R"json(",
                    "caption": "Active Minimum",
                    "defaultValue": 42,
                    "minValue": 0,
                    "maxValue": 42,
                    "isActive": true
                },
                {
                    "type": "SpinBox",
                    "name": ")json" + kActiveMaxValueId + R"json(",
                    "caption": "Active Maximum",
                    "defaultValue": 42,
                    "minValue": 42,
                    "maxValue": 100,
                    "isActive": true
                },
                {
                    "type": "Flow",
                    "caption": "Parameters",
                    "items":
                    [
                        {
                            "type": "Button",
                            "name": ")json" + kShowMessageButtonId + R"json(",
                            "caption": "Show Message...",
                            "isActive": true,
                            "parametersModel": )json" + kParametersModel + R"json(
                        }
                    ]
                },
                {
                    "type": "Flow",
                    "caption": "Webpage",
                    "items":
                    [
                        {
                            "type": "TextField",
                            "name": ")json" + kUrlId + R"json(",
                            "caption": "Url",
                            "defaultValue": "https://example.org/"
                        },
                        {
                            "type": "Button",
                            "name": ")json" + kShowUrlButtonId + R"json(",
                            "caption": "Show Webpage...",
                            "isActive": true
                        },
                        {
                            "type": "CheckBox",
                            "name": ")json" + kUseProxyId + R"json(",
                            "caption": "Use proxy",
                            "defaultValue": false
                        },
                        {
                            "type": "CheckBox",
                            "name": ")json" + kUseDeviceCredentialsId + R"json(",
                            "caption": "Use device credentials",
                            "defaultValue": false
                        }
                    ]
                },
                {
                    "type": "CheckBox",
                    "name": ")json" + kActiveCheckBoxForValueSetChangeId + R"json(",
                    "caption": "Add optional value to combo-box:",
                    "defaultValue": true,
                    "isActive": true
                },
                {
                    "type": "ComboBox",
                    "name": ")json" + kComboBoxForValueSetChangeId + R"json(",
                    "caption": "Values",
                    "defaultValue": ")json" + kComboBoxForValueSetChangeValuePermanent + R"json(",
                    "range":
                    [
                        ")json" + kComboBoxForValueSetChangeValuePermanent + R"json(",
                        ")json" + kComboBoxForValueSetChangeValueOptional + R"json("
                    ]
                }
            ]
        }
    ]
}
)json";

} // namespace settings
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
