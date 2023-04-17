// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include <nx/sdk/analytics/helpers/engine.h>
#include <nx/sdk/analytics/helpers/plugin.h>
#include <nx/sdk/analytics/i_uncompressed_video_frame.h>
#include <nx/sdk/uuid.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace diagnostic_events {

const std::string kGeneratePluginDiagnosticEventsFromEngineSetting =
    "generatePluginDiagnosticEventsFromEngine";

class Engine: public nx::sdk::analytics::Engine
{
public:
    Engine(nx::sdk::analytics::Plugin* plugin);
    virtual ~Engine() override;

    nx::sdk::analytics::Plugin* const plugin() const { return m_plugin; }

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
    nx::sdk::analytics::Plugin* const m_plugin;

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
