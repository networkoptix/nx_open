// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace deprecated_object_detection {

const std::string kGenerateCarsSetting{"generateCars"};
const std::string kGenerateTrucksSetting{"generateTrucks"};
const std::string kGeneratePedestriansSetting{"generatePedestrians"};
const std::string kGenerateHumanFacesSetting{"generateHumanFaces"};
const std::string kGenerateBicyclesSetting{"generateBicycles"};
const std::string kGenerateStonesSetting{"generateStones"};

const std::string kGenerateObjectsEveryNFramesSetting{"generateObjectsEveryNFrames"};
const std::string kNumberOfObjectsToGenerateSetting{"numberOfObjectsToGenerate"};
// TODO: In Stub, consistently use the term BestShot instead of Preview.
const std::string kGeneratePreviewPacketSetting{"generatePreviewPacket"};
const std::string kPreviewImageFileSetting{"previewImageFile"};
const std::string kGeneratePreviewAfterNFramesSetting("generatePreviewAfterNFrames");

const std::string kAdditionalFrameProcessingDelayMsSetting{"additionalFrameProcessingDelayMs"};
const std::string kOverallMetadataDelayMsSetting{"overallMetadataDelayMs"};

static const std::string kSettingsModelPart1 = /*suppress newline*/ 1 + R"json(
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
)json";

static const std::string kStubObjectTypesSettings = R"json(
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
                        },)json";

static const std::string kSettingsModelPart2 = R"json(
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
                            "type": "TextArea",
                            "name": ")json" + kPreviewImageFileSetting + R"json(",
                            "caption": "Preview image file",
                            "description": "Path to an image which should be used as a preview for tracks"
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

} // namespace deprecated_object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
