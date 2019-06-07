#pragma once

#include <atomic>
#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>

#include <nx/sdk/uuid.h>
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

    Engine(nx::sdk::analytics::IPlugin* plugin);
    virtual ~Engine() override;

    virtual nx::sdk::analytics::IDeviceAgent* obtainDeviceAgent(
        const nx::sdk::IDeviceInfo* deviceInfo, nx::sdk::Error* outError) override;

    // Capabilities.
    bool needUncompressedVideoFrames() const { return m_needUncompressedVideoFrames; }
    PixelFormat pixelFormat() const { return m_pixelFormat; }

protected:
    virtual std::string manifest() const override;

    virtual void settingsReceived() override;

    virtual void executeAction(
        const std::string& actionId,
        nx::sdk::Uuid objectId,
        nx::sdk::Uuid deviceId,
        int64_t timestampUs,
        nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackInfo> objectTrackInfo,
        const std::map<std::string, std::string>& params,
        std::string* outActionUrl,
        std::string* outMessageToUser,
        nx::sdk::Error* error) override;

private:
    void initCapabilities();
    void generatePluginEvents();

private:
    mutable std::mutex m_pluginEventGenerationLoopMutex;
    mutable std::condition_variable m_pluginEventGenerationLoopCondition;

    std::unique_ptr<std::thread> m_thread;
    std::atomic<bool> m_terminated{false};
    std::atomic<bool> m_needToThrowPluginEvents{false};

    std::string m_capabilities;
    bool m_needUncompressedVideoFrames = false;
    PixelFormat m_pixelFormat = PixelFormat::yuv420;
};

static const std::string kGenerateEventsSetting{"stub.generateEvents"};
static const std::string kGenerateObjectsSetting{"stub.generateObjects"};
static const std::string kGenerateObjectsEveryNFramesSetting{"stub.generateObjectsEveryNFrames"};
static const std::string kNumberOfObjectsToGenerateSetting{"stub.numberOfObjectsToGenerate"};
static const std::string kGeneratePreviewPacketSetting{"stub.generatePreviewPacket"};
static const std::string kThrowPluginEventsFromEngineSetting{
    "stub.throwPluginEventsFromDeviceAgent"};
static const std::string kThrowPluginEventsFromDeviceAgentSetting{
    "stub.throwPluginEventsFromDeviceAgent"};

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
