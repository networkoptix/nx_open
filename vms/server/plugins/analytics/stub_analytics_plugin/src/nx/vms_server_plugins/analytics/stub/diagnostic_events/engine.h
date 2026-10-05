// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include <nx/sdk/analytics/helpers/engine.h>
#include <nx/sdk/analytics/helpers/integration.h>
#include <nx/sdk/analytics/i_uncompressed_video_frame.h>
#include <nx/sdk/uuid.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace diagnostic_events {

const std::string kGenerateIntegrationDiagnosticEventsFromEngineSetting =
    "generateIntegrationDiagnosticEventsFromEngine";

class Engine: public nx::sdk::analytics::Engine
{
public:
    Engine(nx::sdk::analytics::Integration* integration);
    virtual ~Engine() override;

    nx::sdk::analytics::Integration* const integration() const { return m_integration; }

protected:
    virtual std::string manifestString() const override;

    virtual nx::sdk::Result<const nx::sdk::ISettingsResponse*> settingsReceived() override;

protected:
    virtual void doObtainDeviceAgent(
        nx::sdk::Result<nx::sdk::analytics::IDeviceAgent*>* outResult,
        const nx::sdk::IDeviceInfo* deviceInfo) override;

private:
    void eventThreadLoop();
    void startEventThread();
    void stopEventThread();

private:
    nx::sdk::analytics::Integration* const m_integration;

    std::unique_ptr<std::thread> m_eventThread;
    std::mutex m_eventThreadMutex;
    std::condition_variable m_eventThreadCondition;
    std::atomic<bool> m_terminated{false};

    struct EngineSettings
    {
        std::atomic<bool> generateEvents{false};
    };

    EngineSettings m_engineSettings;
};

} // namespace diagnostic_events
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
