// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace special_objects {

const std::string kGenerateFixedObjectSetting{"generateFixedObject"};
const std::string kFixedObjectColorSetting{"fixedObjectColor"};
const std::string kNoSpecialColorSettingValue{"No special color"};

const std::string kGenerateCounterSetting{"generateCounter"};
const std::string kCounterBoundingBoxSideSizeSetting{"counterBoundingBoxSideSize"};
const std::string kCounterXOffsetSetting{"counterXOffset"};
const std::string kCounterYOffsetSetting{"counterYOffset"};

const std::string kBlinkingObjectPeriodMsSetting{"blinkingObjectPeriodMs"};
const std::string kBlinkingObjectInDedicatedPacketSetting{"blinkingObjectInDedicatedPacket"};

const std::string kGenerateObjectsEveryNFramesSetting{"generateObjectsEveryNFrames"};
const std::string kAdditionalFrameProcessingDelayMsSetting{"additionalFrameProcessingDelayMs"};
const std::string kOverallMetadataDelayMsSetting{"overallMetadataDelayMs"};

static const std::string kSettingsModel = /*suppress newline*/ 1 + R"json(
{
    "type": "Settings",
    "items":
    [
        {
            "type": "GroupBox",
            "caption": "Stub DeviceAgent settings",
            "items":
            [
                {
                    "type": "GroupBox",
                    "caption": "Object generation settings",
                    "items":
                    [
                        {
                            "type": "CheckBox",
                            "name": ")json" + kGenerateFixedObjectSetting + R"json(",
                            "caption": "Generate fixed object",
                            "description": "Generates a fixed object with coordinates (0.25, 0.25, 0.25, 0.25)",
                            "defaultValue": false
                        },
                        {
                            "type": "ComboBox",
                            "name": ")json" + kFixedObjectColorSetting + R"json(",
                            "caption": "Fixed object color",
                            "range": [
                                ")json" + kNoSpecialColorSettingValue + R"json(",
                                "Magenta", "Blue", "Green", "Yellow", "Cyan", "Purple", "Orange",
                                "Red", "White", "#FFFFC0", "!invalid!", "#NONHEX"
                            ],
                            "defaultValue": ")json" + kNoSpecialColorSettingValue + R"json("
                        },
                        {
                            "type": "CheckBox",
                            "name": ")json" + kGenerateCounterSetting + R"json(",
                            "caption": "Generate counter",
                            "description": "Generates a counter",
                            "defaultValue": false
                        },
                        {
                            "type": "DoubleSpinBox",
                            "caption": "Size of the side of the counter bounding box",
                            "name": ")json" + kCounterBoundingBoxSideSizeSetting + R"json(",
                            "defaultValue": 0.0,
                            "minValue": 0.0,
                            "maxValue": 1.0
                        },
                        {
                            "type": "DoubleSpinBox",
                            "caption": "Counter bounding box X-Offset",
                            "name": ")json" + kCounterXOffsetSetting + R"json(",
                            "defaultValue": 0.0,
                            "minValue": 0.0,
                            "maxValue": 1.0
                        },
                        {
                            "type": "DoubleSpinBox",
                            "caption": "Counter bounding box Y-Offset",
                            "name": ")json" + kCounterYOffsetSetting + R"json(",
                            "defaultValue": 0.0,
                            "minValue": 0.0,
                            "maxValue": 1.0
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
                            "name": ")json" + kGenerateObjectsEveryNFramesSetting + R"json(",
                            "caption": "Generate objects every N frames",
                            "defaultValue": 1,
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
                    "type": "SpinBox",
                    "name": ")json" + kAdditionalFrameProcessingDelayMsSetting + R"json(",
                    "caption": "Additional frame processing delay, ms",
                    "defaultValue": 0,
                    "minValue": 0,
                    "maxValue": 1000000000
                }
            ]
        }
    ]
}
)json";

} // namespace special_objects
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
