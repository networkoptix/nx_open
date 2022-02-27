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
        }
    ]
}
)json";

} // namespace settings
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
