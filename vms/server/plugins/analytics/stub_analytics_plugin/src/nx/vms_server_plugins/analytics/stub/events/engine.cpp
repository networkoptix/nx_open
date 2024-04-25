// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include <nx/sdk/helpers/settings_response.h>

#include "device_agent.h"
#include "stub_analytics_plugin_events_ini.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace events {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine(Plugin* plugin):
    nx::sdk::analytics::Engine(NX_DEBUG_ENABLE_OUTPUT, plugin->instanceId()),
    m_plugin(plugin)
{
}

Engine::~Engine()
{
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    *outResult = new DeviceAgent(this, deviceInfo);
}

static std::string buildCapabilities()
{
    if (ini().deviceDependent)
        return "deviceDependent";

    return "";
}

std::string Engine::manifestString() const
{
    std::string result = /*suppress newline*/ 1 + (const char*)R"json(
{
    "typeLibrary":
    {
        "eventTypes":
        [
            {
                "id": ")json" + kLineCrossingEventType + R"json(",
                "name": "Line crossing"
            },
            {
                "id": ")json" + kObjectInTheAreaEventType + R"json(",
                "name": "Object in the area",
                "flags": "stateDependent|regionDependent"
            },
            {
                "id": ")json" + kSuspiciousNoiseEventType + R"json(",
                "name": "Suspicious noise",
                "groupId": ")json" + kSoundRelatedEventGroup + R"json("
            }
        ],
        "groups":
        [
            {
                "id": ")json" + kSoundRelatedEventGroup + R"json(",
                "name": "Sound related events"
            }
        ]
    },
    "capabilities": ")json" + buildCapabilities() + R"json(",
    "deviceAgentSettingsModel":
    {
        "type": "Settings",
        "items":
        [
            {
                "type": "CheckBox",
                "caption": "Declare additional event types",
                "name": ")json" + kDeclareAdditionalEventTypesSetting + R"json(",
                "defaultValue": false
            },
            {
                "type": "CheckBox",
                "name": ")json" + kGenerateEventsSetting + R"json(",
                "caption": "Generate events",
                "defaultValue": true
            }
        ]
    }
}
)json";

    return result;
}

} // namespace events
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
