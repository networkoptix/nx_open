// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include <nx/sdk/i_device_info.h>
#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/error.h>
#include <nx/sdk/helpers/settings_response.h>

#include "../utils.h"

#include "device_agent.h"
#include "stub_analytics_plugin_motion_metadata_ini.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace motion_metadata {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

using namespace std::chrono;
using namespace std::literals::chrono_literals;

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
    std::string capabilities;

    if (ini().deviceDependent)
        capabilities += "|deviceDependent";

    if (ini().keepObjectBoundingBoxRotation)
        capabilities += "|keepObjectBoundingBoxRotation";

    // Delete first '|', if any.
    if (!capabilities.empty() && capabilities.at(0) == '|')
        capabilities.erase(0, 1);

    return capabilities;
}

std::string Engine::manifestString() const
{
    std::string result = /*suppress newline*/ 1 + (const char*)
R"json(
{
    "typeLibrary":
    {
        "objectTypes":
        [
            {
                "id": ")json" + kMotionVisualizationObjectType + R"json(",
                "name": "Stub: Motion visualization object",
                "_comment": "Such Objects are generated to visualize incoming Motion data."
            }
        ]
    },
    "capabilities": ")json" + buildCapabilities() + R"json(",
    "streamTypeFilter": "motion|compressedVideo",
    "deviceAgentSettingsModel":
    {
        "type": "Settings",
        "items":
        [
            {
                "type": "SpinBox",
                "name": ")json" + kObjectWidthInMotionCellsSetting + R"json(",
                "caption": "Generated Object width expressed in motion cells",
                "defaultValue": 8,
                "minValue": 1,
                "maxValue": 1000000000
            },
            {
                "type": "SpinBox",
                "name": ")json" + kObjectHeightInMotionCellsSetting + R"json(",
                "caption": "Generated Object height expressed in motion cells",
                "defaultValue": 8,
                "minValue": 1,
                "maxValue": 1000000000
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
}
)json";

    return result;
}

} // namespace motion_metadata
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
