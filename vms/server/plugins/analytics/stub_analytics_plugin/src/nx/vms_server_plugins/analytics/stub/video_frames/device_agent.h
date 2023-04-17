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
#include "stub_analytics_plugin_video_frames_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace video_frames {

const std::string kMotionVisualizationObjectType = "nx.stub.motionVisualization";
const std::string kAdditionalFrameProcessingDelayMsSetting = "additionalFrameProcessingDelayMs";
const std::string kLeakFramesSetting = "leakFrames";

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

    virtual bool pushCompressedVideoFrame(
        const nx::sdk::analytics::ICompressedVideoPacket* videoFrame) override;

    virtual bool pushUncompressedVideoFrame(
        const nx::sdk::analytics::IUncompressedVideoFrame* videoFrame) override;

private:
    void processVideoFrame(const nx::sdk::analytics::IDataPacket* videoFrame, const char* func);

    bool checkVideoFrame(const nx::sdk::analytics::IUncompressedVideoFrame* frame) const;

    bool checkVideoFramePlane(
        const nx::sdk::analytics::IUncompressedVideoFrame* frame,
        const nx::sdk::analytics::PixelFormatDescriptor* pixelFormatDescriptor,
        int plane) const;

    void dumpSomeFrameBytes(
        const nx::sdk::analytics::IUncompressedVideoFrame* frame,
        int plane) const;

private:
    Engine* const m_engine;

    int m_frameCounter = 0;

    struct DeviceAgentSettings
    {
        std::atomic<bool> leakFrames{false};

        std::atomic<std::chrono::milliseconds> additionalFrameProcessingDelayMs{
            std::chrono::milliseconds::zero()};
    };

    DeviceAgentSettings m_deviceAgentSettings;
};

} // namespace video_frames
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
