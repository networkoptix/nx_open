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
    "deviceAgentSettingsModel":
    {
        "type": "Settings",
        "items":
        [
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
                        "name": ")json" + kObjectCountSetting + R"json(",
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
                        "name": ")json" + kTopLeftXSetting + R"json(",
                        "defaultValue": 0.0,
                        "minValue": 0.0,
                        "maxValue": 1.0
                    },
                    {
                        "type": "DoubleSpinBox",
                        "caption": "Vertical shift",
                        "name": ")json" + kTopLeftYSetting + R"json(",
                        "defaultValue": 0.0,
                        "minValue": 0.0,
                        "maxValue": 1.0
                    },
                    {
                        "type": "DoubleSpinBox",
                        "caption": "Width",
                        "name": ")json" + kWidthSetting + R"json(",
                        "defaultValue": 0.2,
                        "minValue": 0.0,
                        "maxValue": 1.0
                    },
                    {
                        "type": "DoubleSpinBox",
                        "caption": "Height",
                        "name": ")json" + kHeightSetting + R"json(",
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
                        "name": ")json" + kUrlSetting + R"json(",
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
                        "name": ")json" + kImagePathSetting + R"json(",
                        "caption": "Path to Best Shot image"
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
