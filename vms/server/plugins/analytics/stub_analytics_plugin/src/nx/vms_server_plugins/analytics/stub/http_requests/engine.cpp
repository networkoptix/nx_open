// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include "device_agent.h"
#include "settings.h"

namespace nx::vms_server_plugins::analytics::stub::http_requests {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine(nx::sdk::analytics::Plugin* plugin):
    nx::sdk::analytics::Engine(/*enableOutput*/ true), m_plugin(plugin)
{
}

Engine::~Engine()
{
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    *outResult = new DeviceAgent(this, deviceInfo);
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
                "caption": "HTTP request setup",
                "items":
                [
                    {
                        "type": "ComboBox",
                        "name": ")json" + kHttpDomainVar + R"json(",
                        "caption": "HTTP request domain",
                        "defaultValue": ")json" + kHttpVmsDomain + R"json(",
                        "range":
                        [
                            ")json" + kHttpCloudDomain + R"json(",
                            ")json" + kHttpVmsDomain + R"json("
                        ],
                        "itemCaptions":
                        {
                            ")json" + kHttpCloudDomain + R"json(": "Cloud server",
                            ")json" + kHttpVmsDomain + R"json(": "VMS server"
                        }
                    },
                    {
                        "type": "SpinBox",
                        "name": ")json" + kHttpRequestTimePeriodSeconds + R"json(",
                        "caption": "HTTP request time period in seconds",
                        "minValue": 1,
                        "defaultValue": 1
                    },
                    {
                        "type": "TextField",
                        "name": ")json" + kHttpUrlVar + R"json(",
                        "caption": "HTTP endpoint path with optional query and fragment",
                        "defaultValue": "/rest/v1/system/settings"
                    },
                    {
                        "type": "TextField",
                        "name": ")json" + kHttpMethodVar + R"json(",
                        "caption": "HTTP method type",
                        "defaultValue": "GET"
                    },
                    {
                        "type": "TextField",
                        "name": ")json" + kHttpMimeTypeVar + R"json(",
                        "caption": "HTTP request MIME type",
                        "defaultValue": "text/plain"
                    },
                    {
                        "type": "TextField",
                        "name": ")json" + kHttpRequestBodyVar + R"json(",
                        "caption": "HTTP request body",
                        "defaultValue": ""
                    }
                ]
            }
        ]
    }
}
)json";
}

nx::sdk::Ptr<nx::sdk::IUtilityProvider> Engine::utilityProvider() const
{
    return m_plugin->utilityProvider();
}

} // namespace nx::vms_server_plugins::analytics::stub::http_requests
