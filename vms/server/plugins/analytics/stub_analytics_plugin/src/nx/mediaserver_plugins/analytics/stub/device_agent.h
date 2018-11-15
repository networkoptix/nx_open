#pragma once

#include <thread>
#include <atomic>
#include <memory>
#include <condition_variable>

#include <nx/sdk/analytics/common_video_frame_processing_device_agent.h>

#include "engine.h"

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace stub {

class DeviceAgent: public nx::sdk::analytics::CommonVideoFrameProcessingDeviceAgent
{
public:
    DeviceAgent(Engine* engine);

    virtual nx::sdk::Error setNeededMetadataTypes(
        const nx::sdk::analytics::IMetadataTypes* neededMetadataTypes) override;

    virtual nx::sdk::Settings* settings() const override;

protected:
    virtual std::string manifest() const override;

    virtual void settingsChanged() override;

    virtual bool pushCompressedVideoFrame(
        const nx::sdk::analytics::CompressedVideoPacket* videoFrame) override;

    virtual bool pushUncompressedVideoFrame(
        const nx::sdk::analytics::UncompressedVideoFrame* videoFrame) override;

    virtual bool pullMetadataPackets(
        std::vector<nx::sdk::analytics::MetadataPacket*>* metadataPackets) override;

private:
    virtual Engine* engine() const override { return engineCasted<Engine>(); }

    nx::sdk::analytics::MetadataPacket* cookSomeEvents();
    nx::sdk::analytics::MetadataPacket* cookSomeObjects();

    int64_t usSinceEpoch() const;

    bool checkFrame(const nx::sdk::analytics::UncompressedVideoFrame* videoFrame) const;

    // TODO: #dmishin Get rid of these methods. 
    virtual nx::sdk::Error startFetchingMetadata(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes) override;

    virtual void stopFetchingMetadata() override;

private:
    std::unique_ptr<std::thread> m_thread;
    std::atomic<bool> m_stopping{false};
    std::condition_variable m_eventGenerationLoopCondition;
    std::mutex m_eventGenerationLoopMutex;

    bool m_previewAttributesGenerated = false;
    int m_frameCounter = 0;
    int m_counter = 0;
    int m_objectCounter = 0;
    int m_currentObjectIndex = -1;
    nxpl::NX_GUID m_objectId;
    std::string m_eventTypeId;
    int64_t m_lastVideoFrameTimestampUsec = 0;
};

const std::string kLineCrossingEventType = "nx.stub.lineCrossing";
const std::string kObjectInTheAreaEventType = "nx.stub.objectInTheArea";
const std::string kCarObjectType = "nx.stub.car";
const std::string kHumanFaceObjectType = "nx.stub.humanFace";

} // namespace stub
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
