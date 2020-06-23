#pragma once

#include <string>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

const std::string kGenerateEventsSetting{"generateEvents"};
const std::string kMotionVisualizationObjectType{"nx.stub.motionVisualization"};

const std::string kGenerateCarsSetting{"generateCars"};
const std::string kGenerateTrucksSetting{"generateTrucks"};
const std::string kGeneratePedestriansSetting{"generatePedestrians"};
const std::string kGenerateHumanFacesSetting{"generateHumanFaces"};
const std::string kGenerateBicyclesSetting{"generateBicycles"};
const std::string kGenerateStonesSetting{"generateStones"};
const std::string kGenerateFixedObjectSetting{"generateFixedObject"};
const std::string kGenerateCounterSetting{"generateCounter"};

const std::string kBlinkingObjectPeriodMsSetting{"blinkingObjectPeriodMs"};
const std::string kBlinkingObjectInDedicatedPacketSetting{"blinkingObjectInDedicatedPacket"};

const std::string kGenerateObjectsEveryNFramesSetting{"generateObjectsEveryNFrames"};
const std::string kNumberOfObjectsToGenerateSetting{"numberOfObjectsToGenerate"};
const std::string kGeneratePreviewPacketSetting{"generatePreviewPacket"};
const std::string kGeneratePreviewAfterNFramesSetting("generatePreviewAfterNFrames");
const std::string kThrowPluginDiagnosticEventsFromDeviceAgentSetting{
    "throwPluginDiagnosticEventsFromDeviceAgent"};

const std::string kThrowPluginDiagnosticEventsFromEngineSetting{
    "throwPluginDiagnosticEventsFromDeviceAgent"};
const std::string kDisableStreamSelectionSetting{"disableStreamSelection"};
const std::string kLeakFramesSetting{"leakFrames"};
const std::string kAdditionalFrameProcessingDelayMsSetting{"additionalFrameProcessingDelayMs"};
const std::string kOverallMetadataDelayMsSetting{"overallMetadataDelayMs"};
const std::string kUsePluginAsSettingsOriginForDeviceAgents{
    "usePluginAsSettingsOriginForDeviceAgents"};

static const std::string kRegularSettingsModelOption = "regular";
static const std::string kAlternativeSettingsModelOption = "alternative";

static const std::string kSettingsModelSettings = "settingsModelComboBox";

static const std::string kLanguageSelector = "languageSelectorSettings";
static const std::string kEnglishOption = "English";
static const std::string kGermanOption = "Deutsch";

static const std::string kAlternativeSettingsModel =
    /*suppress newline*/ 1 + (const char*) R"json("
{
    "type": "Settings",
    "items": [
        {
            "type": "ComboBox",
            "name": ")json" + kSettingsModelSettings + R"json(",
            "caption": "Settings model",
            "defaultValue": ")json" + kRegularSettingsModelOption + R"json(",
            "range": [
                ")json" + kRegularSettingsModelOption + R"json(",
                ")json" + kAlternativeSettingsModelOption + R"json("
            ]
        },
        {
            "type": "GroupBox",
            "caption": "Alternative GroupBox",
            "items": [
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
})json";

static const std::string kRegularSettingsModelPart1 = /*suppress newline*/ 1 + R"json(
{
    "type": "Settings",
    "items": [
        {
            "type": "ComboBox",
            "name": ")json" + kSettingsModelSettings + R"json(",
            "caption": "Settings model",
            "defaultValue": ")json" + kRegularSettingsModelOption + R"json(",
            "range": [
                ")json" + kRegularSettingsModelOption + R"json(",
                ")json" + kAlternativeSettingsModelOption + R"json("
            ]
        },
        {
            "type": "GroupBox",
            "caption": "Real Stub DeviceAgent settings",
            "items": [
                {
                    "type": "GroupBox",
                    "caption": "Object generation settings",
                    "items": [
                        {
                            "type": "CheckBox",
                            "name": ")json" + kGenerateCarsSetting + R"json(",
                            "caption": "Generate cars",
                            "defaultValue": true
                        },
                        {
                            "type": "CheckBox",
                            "name": ")json" + kGenerateTrucksSetting + R"json(",
                            "caption": "Generate trucks",
                            "defaultValue": true
                        },
                        {
                            "type": "CheckBox",
                            "name": ")json" + kGeneratePedestriansSetting + R"json(",
                            "caption": "Generate pedestrians",
                            "defaultValue": true
                        },
                        {
                            "type": "CheckBox",
                            "name": ")json" + kGenerateHumanFacesSetting + R"json(",
                            "caption": "Generate human faces",
                            "defaultValue": true
                        },
                        {
                            "type": "CheckBox",
                            "name": ")json" + kGenerateBicyclesSetting + R"json(",
                            "caption": "Generate bicycles",
                            "defaultValue": true
                        },
                        {
                            "type": "CheckBox",
                            "name": ")json" + kGenerateStonesSetting + R"json(",
                            "caption": "Generate stones",
                            "defaultValue": false
                        },
                        {
                            "type": "CheckBox",
                            "name": ")json" + kGenerateFixedObjectSetting + R"json(",
                            "caption": "Generate fixed object",
                            "description": "Generates a fixed object with coordinates (0.25, 0.25, 0.25, 0.25)",
                            "defaultValue": false
                        },
                        {
                            "type": "CheckBox",
                            "name": ")json" + kGenerateCounterSetting + R"json(",
                            "caption": "Generate counter",
                            "description": "Generates a counter",
                            "defaultValue": false
                        },
                        {
                            "type": "SpinBox",
                            "name": ")json" + kBlinkingObjectPeriodMsSetting + R"json(",
                            "caption": "Generate 1-frame BlinkingObject every N ms (if not 0)",
                            "defaultValue": 0,
                            "minValue": 0,
                            "maxValue": 100000
                        },
                        {
                            "type": "CheckBox",
                            "name": ")json" + kBlinkingObjectInDedicatedPacketSetting + R"json(",
                            "caption": "Put BlinkingObject into a dedicated MetadataPacket",
                            "defaultValue": false
                        },
                        {
                            "type": "SpinBox",
                            "name": ")json" + kNumberOfObjectsToGenerateSetting + R"json(",
                            "caption": "Number of objects to generate",
                            "defaultValue": 1,
                            "minValue": 1,
                            "maxValue": 100000
                        },
                        {
                            "type": "SpinBox",
                            "name": ")json" + kGenerateObjectsEveryNFramesSetting + R"json(",
                            "caption": "Generate objects every N frames",
                            "defaultValue": 1,
                            "minValue": 1,
                            "maxValue": 100000
                        },
                        {
                            "type": "CheckBox",
                            "name": ")json" + kGeneratePreviewPacketSetting + R"json(",
                            "caption": "Generate preview packet",
                            "defaultValue": true
                        },
                        {
                            "type": "SpinBox",
                            "name": ")json" + kGeneratePreviewAfterNFramesSetting + R"json(",
                            "caption": "Generate preview after N frames",
                            "defaultValue": 30,
                            "minValue": 1,
                            "maxValue": 100000
                        },
                        {
                            "type": "SpinBox",
                            "name": ")json" + kOverallMetadataDelayMsSetting + R"json(",
                            "caption": "Overall metadata delay, ms",
                            "defaultValue": 0,
                            "minValue": 0,
                            "maxValue": 1000000000
                        }
                    ]
                },
                {
                    "type": "CheckBox",
                    "name": ")json" + kGenerateEventsSetting + R"json(",
                    "caption": "Generate events",
                    "defaultValue": true
                },
                {
                    "type": "CheckBox",
                    "name": ")json" + kThrowPluginDiagnosticEventsFromDeviceAgentSetting + R"json(",
                    "caption": "Produce Plugin Diagnostic Events from the DeviceAgent",
                    "defaultValue": false
                },
                {
                    "type": "CheckBox",
                    "name": ")json" + kLeakFramesSetting + R"json(",
                    "caption": "Force a memory leak when processing a video frame",
                    "defaultValue": false
                },
                {
                    "type": "SpinBox",
                    "name": ")json" + kAdditionalFrameProcessingDelayMsSetting + R"json(",
                    "caption": "Additional frame processing delay, ms",
                    "defaultValue": 0,
                    "minValue": 0,
                    "maxValue": 1000000000
                }
            ]
        },
        {
            "type": "GroupBox",
            "caption": "Example Stub DeviceAgent settings",
            "items": [
                {
                    "type": "TextField",
                    "name": "testTextField",
                    "caption": "Device Agent Text Field",
                    "description": "A text field",
                    "defaultValue": "a text"
                },
                {
                    "type": "ComboBox",
                    "name": ")json" + kLanguageSelector + R"json(",
                    "caption": "Language",
                    "defaultValue": "English",
                    "range": [
                        ")json" + kEnglishOption + R"json(",
                        ")json" + kGermanOption + R"json("
                ]
                },)json";

static const std::string kEnglishLanguagePart = /*suppress newline*/ 1 + R"json(
                {
                    "type": "RadioButtonGroup",
                    "name": "testEnglishRadioButtonGroup",
                    "caption": "Choose one",
                    "description": "Choose one option",
                    "defaultValue": "Yes",
                    "range": [
                        "Yes",
                        "No"
                    ]
                },)json";

static const std::string kGermanLanguagePart = /*suppress newline*/ 1 + R"json(
                {
                    "type": "RadioButtonGroup",
                    "name": "testGermanRadioButtonGroup",
                    "caption": "Choose one",
                    "description": "Choose one option",
                    "defaultValue": "Ja",
                    "range": [
                        "Ja",
                        "Nein",
                        "Doch"
                    ]
                },)json";

static const std::string kRegularSettingsModelPart2 = /*suppress newline*/ 1 + R"json("
                {
                    "type": "RadioButtonGroup",
                    "name": "testRadioButtonGroup",
                    "caption": "RadioButton Group",
                    "description": "Choose one option",
                    "defaultValue": "Cs_enodatum",
                    "range": [
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
                    "itemCaptions": {
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
                    "range": [
                        "Coleoidea",
                        "Ammonoidea",
                        "Nautiloidea",
                        "Orthoceratoidea"
                    ],
                    "itemCaptions": {
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
                    "itemCaptions": {
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
    "sections": [
        {
            "type": "Section",
            "caption": "Example",
            "items": [
                {
                    "type": "GroupBox",
                    "caption": "Example Stub DeviceAgent settings",
                    "items": [
                        {
                            "type": "TextField",
                            "name": "testTextFieldWithValidation",
                            "caption": "Hexadecimal number text field",
                            "defaultValue": "12ab34cd",
                            "validationRegex": "^[a-f0-9]+$",
                            "validationRegexFlags": "i",
                            "validationErrorMessage": "Text must contain only digits and characters a-f, e.g. 12ab34cd."
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
            "sections": [
                {
                    "type": "Section",
                    "caption": "Nested section",
                    "items": [
                        {
                            "type": "GroupBox",
                            "caption": "Nested Section Example",
                            "items": [
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
        },)json" R"json(
        {
            "type": "Section",
            "caption": "ROI",
            "items": [
                {
                    "type": "GroupBox",
                    "caption": "Polygons",
                    "items": [
                        {
                            "type": "Repeater",
                            "count": 5,
                            "template": {
                                "type": "GroupBox",
                                "caption": "Polygon #",
                                "filledCheckItems": ["polygon#.figure"],
                                "items": [
                                    {
                                        "type": "PolygonFigure",
                                        "name": "polygon#.figure",
                                        "minPoints": 4,
                                        "maxPoints": 8
                                    },
                                    {
                                        "type": "SpinBox",
                                        "name": "polygon#.threshold",
                                        "caption": "Level of detection",
                                        "defaultValue": 50,
                                        "minValue": 1,
                                        "maxValue": 100
                                    },
                                    {
                                        "type": "SpinBox",
                                        "name": "polygon#.sensitivity",
                                        "caption": "Sensitivity",
                                        "defaultValue": 80,
                                        "minValue": 1,
                                        "maxValue": 100
                                    },
                                    {
                                        "type": "SpinBox",
                                        "name": "polygon#.minimumDuration",
                                        "caption": "Minimum duration (s)",
                                        "defaultValue": 0,
                                        "minValue": 0,
                                        "maxValue": 5
                                    }
                                ]
                            }
                        }
                    ]
                },
                {
                    "type": "GroupBox",
                    "caption": "Boxes",
                    "items": [
                        {
                            "type": "Repeater",
                            "count": 5,
                            "template": {
                                "type": "GroupBox",
                                "caption": "Box #",
                                "filledCheckItems": ["box#.figure"],
                                "items": [
                                    {
                                        "type": "BoxFigure",
                                        "name": "box#.figure"
                                    },
                                    {
                                        "type": "SpinBox",
                                        "name": "box#.threshold",
                                        "caption": "Level of detection",
                                        "defaultValue": 50,
                                        "minValue": 1,
                                        "maxValue": 100
                                    },
                                    {
                                        "type": "SpinBox",
                                        "name": "box#.sensitivity",
                                        "caption": "Sensitivity",
                                        "defaultValue": 80,
                                        "minValue": 1,
                                        "maxValue": 100
                                    },
                                    {
                                        "type": "SpinBox",
                                        "name": "box#.minimumDuration",
                                        "caption": "Minimum duration (s)",
                                        "defaultValue": 0,
                                        "minValue": 0,
                                        "maxValue": 5
                                    }
                                ]
                            }
                        }
                    ]
                },
                {
                    "type": "GroupBox",
                    "caption": "Lines",
                    "items": [
                        {
                            "type": "Repeater",
                            "count": 5,
                            "template": {
                                "type": "GroupBox",
                                "caption": "Line #",
                                "filledCheckItems": ["line#.figure"],
                                "items": [
                                    {
                                        "type": "LineFigure",
                                        "name": "line#.figure"
                                    },
                                    {
                                        "type": "CheckBox",
                                        "name": "line#.person",
                                        "caption": "Person",
                                        "defaultValue": false
                                    },
                                    {
                                        "type": "CheckBox",
                                        "name": "line#.vehicle",
                                        "caption": "Vehicle",
                                        "defaultValue": false
                                    },
                                    {
                                        "type": "CheckBox",
                                        "name": "line#.crossing",
                                        "caption": "Crossing",
                                        "defaultValue": false
                                    }
                                ]
                            }
                        }
                    ]
                },
                {
                    "type": "GroupBox",
                    "caption": "Polyline",
                    "items": [
                        {
                            "type": "LineFigure",
                            "name": "testPolyLine",
                            "caption": "Polyline",
                            "maxPoints": 8
                        }
                    ]
                },
                {
                    "type": "GroupBox",
                    "caption": "Polygon",
                    "items": [
                        {
                            "type": "PolygonFigure",
                            "name": "testPolygon",
                            "caption": "Polygon outside of a repeater",
                            "description": "The points of this polygon are considered as a plugin-side setting",
                            "minPoints": 3,
                            "maxPoints": 8
                        }
                    ]
                },
                {
                    "type": "GroupBox",
                    "caption": "Size Constraints",
                    "items": [
                        {
                            "type": "ObjectSizeConstraints",
                            "name": "testSizeConstraints",
                            "caption": "Object size constraints",
                            "description": "Size range an object should fit into to be detected",
                            "defaultValue": {"minimum": [0.1, 0.4], "maximum": [0.2, 0.8]}
                        }
                    ]
                }
            ]
        }
    ]
})json";

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
