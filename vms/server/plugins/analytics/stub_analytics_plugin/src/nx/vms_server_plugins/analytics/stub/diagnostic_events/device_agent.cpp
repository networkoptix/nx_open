// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <vector>
#include <string>
#include <mutex>
#include <chrono>
#include <ctime>
#include <thread>
#include <type_traits>

#include <nx/kit/utils.h>
#include <nx/kit/json.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_track_best_shot_packet.h>
#include <nx/sdk/analytics/i_motion_metadata_packet.h>
#include <nx/sdk/helpers/error.h>
#include <nx/sdk/helpers/settings_response.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/helpers/uuid_helper.h>

#include "../utils.h"

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

DeviceAgent::DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo):
    ConsumingDeviceAgent(deviceInfo, NX_DEBUG_ENABLE_OUTPUT, engine->plugin()->instanceId()),
    m_engine(engine)
{
    startEventThread();
}

DeviceAgent::~DeviceAgent()
{
    stopEventThread();
}

std::string DeviceAgent::manifestString() const
{
    return /*suppress newline*/ 1 + (const char*) R"json(
{
    "capabilities": "disableStreamSelection"
}
)json";
}

Result<const ISettingsResponse*> DeviceAgent::settingsReceived()
{
    m_deviceAgentSettings.generateEvents =
        toBool(settingValue(kGeneratePluginDiagnosticEventsFromDeviceAgentSetting));

    if (m_deviceAgentSettings.generateEvents)
        NX_PRINT << __func__ << "(): Plugin Diagnostic Event generation enabled via settings.";
    else
        NX_PRINT << __func__ << "(): Plugin Diagnostic Event generation disabled via settings.";

    m_eventThreadCondition.notify_all();

    return nullptr;
}

void DeviceAgent::doSetNeededMetadataTypes(
    Result<void>* /*outResult*/, const IMetadataTypes* /*neededMetadataTypes*/)
{
}

void DeviceAgent::eventThreadLoop()
{
    while (!m_terminated)
    {
        if (m_deviceAgentSettings.generateEvents)
        {
            pushPluginDiagnosticEvent(
                IPluginDiagnosticEvent::Level::info,
                "Info message from DeviceAgent",
                "Info message description");

            pushPluginDiagnosticEvent(
                IPluginDiagnosticEvent::Level::warning,
                "Warning message from DeviceAgent",
                "Warning message description");

            pushPluginDiagnosticEvent(
                IPluginDiagnosticEvent::Level::error,
                "Error message from DeviceAgent",
                "Error message description");
        }

        // Sleep until the next event needs to be generated, or the thread is ordered to
        // terminate (hence condition variable instead of sleep()). Return value (whether
        // the timeout has occurred) and spurious wake-ups are ignored.
        std::unique_lock<std::mutex> lock(m_eventThreadMutex);
        if (m_terminated)
            break;
        {
            static const std::chrono::seconds kEventGenerationPeriod{5};
            m_eventThreadCondition.wait_for(lock, kEventGenerationPeriod);
        }
    }
}

void DeviceAgent::startEventThread()
{
    m_eventThread = std::make_unique<std::thread>([this]() { eventThreadLoop(); });
}

void DeviceAgent::stopEventThread()
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
