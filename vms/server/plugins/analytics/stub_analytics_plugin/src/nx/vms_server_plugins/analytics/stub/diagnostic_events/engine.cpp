// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include <nx/sdk/i_device_info.h>
#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/error.h>
#include <nx/sdk/helpers/settings_response.h>

#include "../utils.h"

#include "device_agent.h"
#include "stub_analytics_plugin_diagnostic_events_ini.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace diagnostic_events {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

using namespace std::chrono;
using namespace std::literals::chrono_literals;

Engine::Engine(Plugin* plugin):
    nx::sdk::analytics::Engine(NX_DEBUG_ENABLE_OUTPUT, plugin->instanceId()),
    m_plugin(plugin)
{
    startEventThread();
}

Engine::~Engine()
{
    stopEventThread();
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
    std::string result = /*suppress newline*/ 1 + (const char*)
R"json(
{
    "capabilities": ")json" + buildCapabilities() + R"json(",
    "deviceAgentSettingsModel":
    {
        "type": "Settings",
        "items":
        [
            {
                "type": "CheckBox",
                "name": ")json" + kGeneratePluginDiagnosticEventsFromDeviceAgentSetting + R"json(",
                "caption": "Generate Plugin Diagnostic Events from the DeviceAgent",
                "defaultValue": false
            }
        ]
    }
}
)json";

    return result;
}

Result<const ISettingsResponse*> Engine::settingsReceived()
{
    m_engineSettings.generateEvents =
        toBool(settingValue(kGeneratePluginDiagnosticEventsFromEngineSetting));

    if (m_engineSettings.generateEvents)
        NX_PRINT << __func__ << "(): Plugin Diagnostic Event generation enabled via settings.";
    else
        NX_PRINT << __func__ << "(): Plugin Diagnostic Event generation disabled via settings.";

    m_eventThreadCondition.notify_all();

    return nullptr;
}

void Engine::eventThreadLoop()
{
    while (!m_terminated)
    {
        if (m_engineSettings.generateEvents)
        {
            pushPluginDiagnosticEvent(
                IPluginDiagnosticEvent::Level::info,
                "Info message from Engine",
                "Info message description");

            pushPluginDiagnosticEvent(
                IPluginDiagnosticEvent::Level::warning,
                "Warning message from Engine",
                "Warning message description");

            pushPluginDiagnosticEvent(
                IPluginDiagnosticEvent::Level::error,
                "Error message from Engine",
                "Error message description");
        }

        // Sleep until the next event pack needs to be generated, or the thread is ordered to
        // terminate (hence condition variable instead of sleep()). Return value (whether the
        // timeout has occurred) and spurious wake-ups are ignored.
        {
            std::unique_lock<std::mutex> lock(m_eventThreadMutex);
            if (m_terminated)
                break;
            static const seconds kEventGenerationPeriod{7};
            m_eventThreadCondition.wait_for(lock, kEventGenerationPeriod);
        }
    }
}

void Engine::startEventThread()
{
    m_eventThread = std::make_unique<std::thread>([this]() { eventThreadLoop(); });
}

void Engine::stopEventThread()
{
    {
        std::unique_lock<std::mutex> lock(m_eventThreadMutex);
        m_terminated = true;
        m_eventThreadCondition.notify_all();
    }

    m_eventThread->join();
}

} // namespace diagnostic_events
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
