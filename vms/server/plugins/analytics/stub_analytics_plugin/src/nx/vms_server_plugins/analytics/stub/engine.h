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
    using Plugin = nx::sdk::analytics::Plugin;

    Engine(Plugin* plugin);
    virtual ~Engine() override;

    virtual nx::sdk::analytics::IDeviceAgent* obtainDeviceAgent(
        const nx::sdk::IDeviceInfo* deviceInfo, nx::sdk::IError* outError) override;

    // Capabilities.
    bool needUncompressedVideoFrames() const { return m_needUncompressedVideoFrames; }
    PixelFormat pixelFormat() const { return m_pixelFormat; }

    virtual Plugin* plugin() const override { return pluginCasted<Plugin>(); }

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
        nx::sdk::IError* outError) override;

private:
    void obtainPluginHomeDir();
    void initCapabilities();
    void generatePluginDiagnosticEvents();

private:
    mutable std::mutex m_pluginDiagnosticEventGenerationLoopMutex;
    mutable std::condition_variable m_pluginDiagnosticEventGenerationLoopCondition;
    std::unique_ptr<std::thread> m_pluginDiagnosticEventThread;
    std::atomic<bool> m_terminated{false};
    std::atomic<bool> m_needToThrowPluginDiagnosticEvents{false};

    std::string m_pluginHomeDir; /**< Can be empty. */
    std::string m_capabilities;
    bool m_needUncompressedVideoFrames = false;
    PixelFormat m_pixelFormat = PixelFormat::yuv420;
};

const std::string kGenerateEventsSetting{"generateEvents"};
const std::string kGenerateCarsSetting{"generateCars"};
const std::string kGenerateTrucksSetting{"generateTrucks"};
const std::string kGeneratePedestriansSetting{"generatePedestrians"};
const std::string kGenerateHumanFacesSetting{"generateHumanFaces"};
const std::string kGenerateBicyclesSetting{"generateBicycles"};

const std::string kGenerateObjectsEveryNFramesSetting{"generateObjectsEveryNFrames"};
const std::string kNumberOfObjectsToGenerateSetting{"numberOfObjectsToGenerate"};
const std::string kGeneratePreviewPacketSetting{"generatePreviewPacket"};
const std::string kThrowPluginDiagnosticEventsFromDeviceAgentSetting{
    "throwPluginDiagnosticEventsFromDeviceAgent"};

const std::string kThrowPluginDiagnosticEventsFromEngineSetting{
    "throwPluginDiagnosticEventsFromDeviceAgent"};

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
