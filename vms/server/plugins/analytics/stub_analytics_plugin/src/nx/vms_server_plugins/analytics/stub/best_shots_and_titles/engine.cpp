// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include "device_agent.h"

#include "settings.h"
#include "stub_analytics_plugin_best_shots_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace best_shots {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine(): nx::sdk::analytics::Engine(ini().enableOutput)
{
}

Engine::~Engine()
{
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    *outResult = new DeviceAgent(deviceInfo);
}

std::string Engine::manifestString() const
{
    return /*suppress newline*/ 1 + (const char*)
R"json(
{
    "streamTypeFilter": "compressedVideo",
    "capabilities": "noAutoBestShots",
    "deviceAgentSettingsModel":
    {
        "type": "Settings",
        "items":
        [
            {
                "type": "GroupBox",
                "caption": "General settings",
                "items":
                [
                    {
                        "type": "CheckBox",
                        "name": ")json" + kEnableBestShotGeneration + R"json(",
                        "caption": "Enable Best Shot generation",
                        "defaultValue": true
                    },
                    {
                        "type": "CheckBox",
                        "name": ")json" + kEnableObjectTitleGeneration + R"json(",
                        "caption": "Enable Object Title generation",
                        "defaultValue": true
                    }
                ]
            },
            {
                "type": "GroupBox",
                "caption": "General Best Shot generation settings",
                "items":
                [
                    {
                        "type": "ComboBox",
                        "name": ")json" + kBestShotGenerationPolicySetting + R"json(",
                        "caption": "Best Shot generation policy",
                        "defaultValue": ")json" + kFixedBoundingBoxBestShotGenerationPolicy + R"json(",
                        "range":
                        [
                            ")json" + kFixedBoundingBoxBestShotGenerationPolicy + R"json(",
                            ")json" + kUrlBestShotGenerationPolicy + R"json(",
                            ")json" + kImageBestShotGenerationPolicy + R"json("
                        ],
                        "itemCaptions":
                        {
                            ")json" + kFixedBoundingBoxBestShotGenerationPolicy + R"json(": "Fixed bounding box",
                            ")json" + kUrlBestShotGenerationPolicy + R"json(": "URL",
                            ")json" + kImageBestShotGenerationPolicy + R"json(": "Image"
                        }
                    },
                    {
                        "type": "SpinBox",
                        "name": ")json" + kBestShotObjectCountSetting + R"json(",
                        "caption": "Object count",
                        "minValue": 0,
                        "defaultValue": 1
                    },
                    {
                        "type": "SpinBox",
                        "name": ")json" + kFrameNumberToGenerateBestShotSetting + R"json(",
                        "caption": "Frame number to generate Best Shot",
                        "minValue": 0,
                        "defaultValue": 0
                    }
                ]
            },
            {
                "type": "GroupBox",
                "caption": "Fixed bounding box Best Shot settings",
                "items":
                [
                    {
                        "type": "DoubleSpinBox",
                        "caption": "Horizontal shift",
                        "name": ")json" + kBestShotTopLeftXSetting + R"json(",
                        "defaultValue": 0.0,
                        "minValue": 0.0,
                        "maxValue": 1.0
                    },
                    {
                        "type": "DoubleSpinBox",
                        "caption": "Vertical shift",
                        "name": ")json" + kBestShotTopLeftYSetting + R"json(",
                        "defaultValue": 0.0,
                        "minValue": 0.0,
                        "maxValue": 1.0
                    },
                    {
                        "type": "DoubleSpinBox",
                        "caption": "Width",
                        "name": ")json" + kBestShotWidthSetting + R"json(",
                        "defaultValue": 0.2,
                        "minValue": 0.0,
                        "maxValue": 1.0
                    },
                    {
                        "type": "DoubleSpinBox",
                        "caption": "Height",
                        "name": ")json" + kBestShotHeightSetting + R"json(",
                        "defaultValue": 0.2,
                        "minValue": 0.0,
                        "maxValue": 1.0
                    }
                ]
            },
            {
                "type": "GroupBox",
                "caption": "URL Best Shot settings",
                "items":
                [
                    {
                        "type": "TextArea",
                        "name": ")json" + kBestShotUrlSetting + R"json(",
                        "caption": "Best Shot URL"
                    }
                ]
            },
            {
                "type": "GroupBox",
                "caption": "Image Best Shot settings",
                "items":
                [
                    {
                        "type": "TextArea",
                        "name": ")json" + kBestShotImagePathSetting + R"json(",
                        "caption": "Path to Best Shot image"
                    }
                ]
            },
            {
                "type": "GroupBox",
                "caption": "General Object Track Title generation settings",
                "defaultValue": ")json" + kFixedBoundingBoxTitleGenerationPolicy + R"json(",
                "items":
                [
                    {
                        "type": "ComboBox",
                        "name": ")json" + kTitleGenerationPolicySetting + R"json(",
                        "caption": "Track Title generation policy",
                        "defaultValue": ")json" + kFixedBoundingBoxTitleGenerationPolicy + R"json(",
                        "range":
                        [
                            ")json" + kFixedBoundingBoxTitleGenerationPolicy + R"json(",
                            ")json" + kUrlTitleGenerationPolicy + R"json(",
                            ")json" + kImageTitleGenerationPolicy + R"json("
                        ],
                        "itemCaptions":
                        {
                            ")json" + kFixedBoundingBoxTitleGenerationPolicy + R"json(": "Fixed bounding box",
                            ")json" + kUrlTitleGenerationPolicy + R"json(": "URL",
                            ")json" + kImageTitleGenerationPolicy + R"json(": "Image"
                        }
                    },
                    {
                        "type": "SpinBox",
                        "name": ")json" + kTitleObjectCountSetting + R"json(",
                        "caption": "Object count",
                        "minValue": 0,
                        "defaultValue": 1
                    },
                    {
                        "type": "SpinBox",
                        "name": ")json" + kFrameNumberToGenerateTitleSetting + R"json(",
                        "caption": "Frame number to generate Object Title",
                        "minValue": 0,
                        "defaultValue": 0
                    }
                ]
            },
            {
                "type": "GroupBox",
                "caption": "Fixed bounding box Object Title settings",
                "items":
                [
                    {
                        "type": "DoubleSpinBox",
                        "caption": "Horizontal shift",
                        "name": ")json" + kTitleTopLeftXSetting + R"json(",
                        "defaultValue": 0.0,
                        "minValue": 0.0,
                        "maxValue": 1.0
                    },
                    {
                        "type": "DoubleSpinBox",
                        "caption": "Vertical shift",
                        "name": ")json" + kTitleTopLeftYSetting + R"json(",
                        "defaultValue": 0.0,
                        "minValue": 0.0,
                        "maxValue": 1.0
                    },
                    {
                        "type": "DoubleSpinBox",
                        "caption": "Width",
                        "name": ")json" + kTitleWidthSetting + R"json(",
                        "defaultValue": 0.2,
                        "minValue": 0.0,
                        "maxValue": 1.0
                    },
                    {
                        "type": "DoubleSpinBox",
                        "caption": "Height",
                        "name": ")json" + kTitleHeightSetting + R"json(",
                        "defaultValue": 0.2,
                        "minValue": 0.0,
                        "maxValue": 1.0
                    }
                ]
            },
            {
                "type": "GroupBox",
                "caption": "URL Title settings",
                "items":
                [
                    {
                        "type": "TextArea",
                        "name": ")json" + kTitleUrlSetting + R"json(",
                        "caption": "Title URL"
                    }
                ]
            },
            {
                "type": "GroupBox",
                "caption": "Image Title settings",
                "items":
                [
                    {
                        "type": "TextArea",
                        "name": ")json" + kTitleImagePathSetting + R"json(",
                        "caption": "Path to Title image"
                    }
                ]
            },
            {
                "type": "GroupBox",
                "caption": "Title Text settings",
                "items":
                [
                    {
                        "type": "TextArea",
                        "name": ")json" + kTitleTextSetting + R"json(",
                        "caption": "Title text"
                    }
                ]
            }
        ]
    }
}
)json";
}

} // namespace best_shots
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
