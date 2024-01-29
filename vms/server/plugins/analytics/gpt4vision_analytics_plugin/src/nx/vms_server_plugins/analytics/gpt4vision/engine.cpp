// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include "device_agent.h"
#include "ini.h"

namespace nx::vms_server_plugins::analytics::gpt4vision {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine():
    nx::sdk::analytics::Engine(ini().enableOutput)
{
}

Engine::~Engine()
{
}

std::string Engine::manifestString() const
{
    return /*suppress newline*/ 1 + (const char*) R"json(
{
    "capabilities": "needUncompressedVideoFrames_rgb",
    "deviceAgentSettingsModel":
    {
        "type": "Settings",
        "items":
        [
            {
                "type": "GroupBox",
                "caption": "Query setup",
                "items":
                [
                    {
                        "type": "TextArea",
                        "name": ")json" + std::string(DeviceAgent::kQueryParameter) + R"json(",
                        "caption": "Query",
                        "defaultValue": "Describe the image"
                    },
                    {
                        "type": "SpinBox",
                        "name": ")json" + std::string(DeviceAgent::kPeriodParameter) + R"json(",
                        "caption": "Query time period in seconds",
                        "minValue": 1,
                        "defaultValue": 15
                    }
                ]
            }
        ]
    }
}
)json";
}

nx::sdk::Result<const nx::sdk::ISettingsResponse*> Engine::settingsReceived()
{
    m_apiKey = settingValue(std::string(Engine::kApiKeyParameter));

    return nullptr;
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    *outResult = new DeviceAgent(this, deviceInfo);
}

} // nx::vms_server_plugins::analytics::gpt4vision
