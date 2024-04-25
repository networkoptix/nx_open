// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include "device_agent.h"
#include "stub_analytics_plugin_video_frames_ini.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace video_frames {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

using namespace std::chrono;
using namespace std::literals::chrono_literals;

Engine::Engine(Plugin* plugin):
    nx::sdk::analytics::Engine(NX_DEBUG_ENABLE_OUTPUT, plugin->instanceId()),
    m_plugin(plugin)
{
    initCapabilities();
}

Engine::~Engine()
{
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    *outResult = new DeviceAgent(this, deviceInfo);
}

void Engine::initCapabilities()
{
    if (ini().deviceDependent)
        m_capabilities += "|deviceDependent";

    m_streamTypeFilter = "compressedVideo";

    const std::string pixelFormatString = ini().needUncompressedVideoFrames;
    if (!pixelFormatString.empty())
    {
        if (!pixelFormatFromStdString(pixelFormatString, &m_pixelFormat))
        {
            NX_PRINT << "ERROR: Invalid value of needUncompressedVideoFrames in "
                << ini().iniFile() << ": [" << pixelFormatString << "].";
        }
        else
        {
            m_needUncompressedVideoFrames = true;
            m_capabilities += std::string("|needUncompressedVideoFrames_") + pixelFormatString;
            m_streamTypeFilter = "uncompressedVideo";
        }
    }

    // Delete first '|', if any.
    if (!m_capabilities.empty() && m_capabilities.at(0) == '|')
        m_capabilities.erase(0, 1);
}

std::string Engine::manifestString() const
{
    std::string result = /*suppress newline*/ 1 + (const char*) R"json(
{
    "capabilities": ")json" + m_capabilities + R"json(",
    "streamTypeFilter": ")json" + m_streamTypeFilter + R"json(",
    "preferredStream": ")json" + ini().preferredStream + R"json(",
    "deviceAgentSettingsModel":
    {
        "type": "Settings",
        "items":
        [
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
    }
}
)json";

    return result;
}

} // namespace video_frames
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
