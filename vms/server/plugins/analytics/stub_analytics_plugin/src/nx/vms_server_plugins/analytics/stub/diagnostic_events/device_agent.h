// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <nx/sdk/analytics/helpers/consuming_device_agent.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/pixel_format.h>

#include "engine.h"
#include "stub_analytics_plugin_diagnostic_events_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace diagnostic_events {

const std::string kGeneratePluginDiagnosticEventsFromDeviceAgentSetting =
    "generatePluginDiagnosticEventsFromDeviceAgent";

class DeviceAgent: public nx::sdk::analytics::ConsumingDeviceAgent
{
public:
    DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo);
    virtual ~DeviceAgent() override;

protected:
    virtual void doSetNeededMetadataTypes(
        nx::sdk::Result<void>* outValue,
        const nx::sdk::analytics::IMetadataTypes* neededMetadataTypes) override;

    virtual std::string manifestString() const override;

    virtual nx::sdk::Result<const nx::sdk::ISettingsResponse*> settingsReceived() override;

private:
    void eventThreadLoop();
    void startEventThread();
    void stopEventThread();

    void processFrameMotion(
        nx::sdk::Ptr<nx::sdk::IList<nx::sdk::analytics::IMetadataPacket>> metadataPacketList);

private:
    Engine* const m_engine;

    std::unique_ptr<std::thread> m_eventThread;
    std::mutex m_eventThreadMutex;
    std::condition_variable m_eventThreadCondition;
    std::atomic<bool> m_terminated{false};

    struct DeviceAgentSettings
    {
        std::atomic<bool> generateEvents{true};
    };

    DeviceAgentSettings m_deviceAgentSettings;
};

} // namespace diagnostic_events
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
