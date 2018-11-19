#pragma once

#include <atomic>
#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>

#include <nx/sdk/analytics/common_engine.h>
#include <nx/sdk/analytics/uncompressed_video_frame.h>

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace stub {

using PixelFormat = nx::sdk::analytics::UncompressedVideoFrame::PixelFormat;

class Engine: public nx::sdk::analytics::CommonEngine
{
public:
    Engine(nx::sdk::analytics::Plugin* plugin);
    virtual ~Engine() override;

    virtual nx::sdk::analytics::DeviceAgent* obtainDeviceAgent(
        const nx::sdk::DeviceInfo* deviceInfo, nx::sdk::Error* outError) override;

    // Capabilities.
    bool needUncompressedVideoFrames() const { return m_needUncompressedVideoFrames; }
    PixelFormat pixelFormat() const { return m_pixelFormat; }

protected:
    virtual std::string manifest() const override;

    virtual void settingsChanged() override;

    virtual void executeAction(
        const std::string& actionId,
        nxpl::NX_GUID objectId,
        nxpl::NX_GUID deviceId,
        int64_t timestampUs,
        const std::map<std::string, std::string>& params,
        std::string* outActionUrl,
        std::string* outMessageToUser,
        nx::sdk::Error* error) override;

private:
    void initCapabilities();
    void processPluginEvents();

private:
    mutable std::mutex m_pluginEventGenerationLoopMutex;
    mutable std::condition_variable m_pluginEventGenerationLoopCondition;

    std::unique_ptr<std::thread> m_thread;
    std::atomic<bool> m_terminated{false};

    std::string m_capabilities;
    bool m_needUncompressedVideoFrames = false;
    PixelFormat m_pixelFormat = PixelFormat::yuv420;
};

} // namespace stub
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
