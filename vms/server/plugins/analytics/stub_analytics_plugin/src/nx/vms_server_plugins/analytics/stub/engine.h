// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>

#include <nx/sdk/uuid.h>
#include <nx/sdk/analytics/helpers/plugin.h>
#include <nx/sdk/analytics/helpers/engine.h>
#include <nx/sdk/analytics/i_uncompressed_video_frame.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

class Engine: public nx::sdk::analytics::Engine
{
public:
    using PixelFormat = nx::sdk::analytics::IUncompressedVideoFrame::PixelFormat;

    Engine(nx::sdk::analytics::Plugin* plugin);

    virtual ~Engine() override;

    // Capabilities.
    bool needUncompressedVideoFrames() const { return m_needUncompressedVideoFrames; }

    PixelFormat pixelFormat() const { return m_pixelFormat; }

    std::string capabilities() const;

protected:
    virtual std::string manifestString() const override;

    virtual nx::sdk::Result<const nx::sdk::ISettingsResponse*> settingsReceived() override;

protected:
    virtual void doObtainDeviceAgent(
        nx::sdk::Result<nx::sdk::analytics::IDeviceAgent*>* outResult,
        const nx::sdk::IDeviceInfo* deviceInfo) override;

    virtual nx::sdk::Result<sdk::analytics::IAction::Result> executeAction(
        const std::string& actionId,
        nx::sdk::Uuid trackId,
        nx::sdk::Uuid deviceId,
        int64_t timestampUs,
        nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackInfo> objectTrackInfo,
        const std::map<std::string, std::string>& params) override;

    virtual void getPluginSideSettings(
        nx::sdk::Result<const nx::sdk::ISettingsResponse*>* outResult) const override;

private:
    void obtainServerSdkVersion();
    void obtainPluginHomeDir();
    void initCapabilities();
    void generatePluginDiagnosticEvents();

private:
    nx::sdk::analytics::Plugin* const m_plugin;

    mutable std::mutex m_pluginDiagnosticEventGenerationLoopMutex;
    mutable std::condition_variable m_pluginDiagnosticEventGenerationLoopCondition;
    std::unique_ptr<std::thread> m_pluginDiagnosticEventThread;
    std::atomic<bool> m_terminated{false};
    std::atomic<bool> m_needToThrowPluginDiagnosticEvents{false};
    std::atomic<bool> m_disableStreamSelection{false};

    std::string m_pluginHomeDir; /**< Can be empty. */
    std::string m_capabilities;
    std::string m_streamTypeFilter;
    bool m_needUncompressedVideoFrames = false;
    PixelFormat m_pixelFormat = PixelFormat::yuv420;
};

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
