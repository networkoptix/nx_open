#pragma once

#include <thread>
#include <atomic>
#include <memory>
#include <condition_variable>
#include <vector>
#include <string>

#include <nx/sdk/uuid.h>
#include <nx/sdk/analytics/helpers/video_frame_processing_device_agent.h>

#include "engine.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

class DeviceAgent: public nx::sdk::analytics::VideoFrameProcessingDeviceAgent
{
public:
    DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo);
    virtual ~DeviceAgent() override;

    virtual nx::sdk::Error setNeededMetadataTypes(
        const nx::sdk::analytics::IMetadataTypes* neededMetadataTypes) override;

    virtual nx::sdk::IStringMap* pluginSideSettings() const override;

protected:
    virtual std::string manifest() const override;

    virtual void settingsReceived() override;

    virtual bool pushCompressedVideoFrame(
        const nx::sdk::analytics::ICompressedVideoPacket* videoFrame) override;

    virtual bool pushUncompressedVideoFrame(
        const nx::sdk::analytics::IUncompressedVideoFrame* videoFrame) override;

    virtual bool pullMetadataPackets(
        std::vector<nx::sdk::analytics::IMetadataPacket*>* metadataPackets) override;

private:
    virtual Engine* engine() const override { return engineCasted<Engine>(); }

    nx::sdk::analytics::IMetadataPacket* cookSomeEvents();
    nx::sdk::analytics::IMetadataPacket* cookSomeObjects();

    int64_t usSinceEpoch() const;

    void processVideoFrame(const nx::sdk::analytics::IDataPacket* videoFrame, const char* func);

    bool checkVideoFrame(const nx::sdk::analytics::IUncompressedVideoFrame* frame) const;

    nx::sdk::Error startFetchingMetadata(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes);

    void stopFetchingMetadata();

    void processPluginEvents();

    void generateObjectIds();

private:
    std::unique_ptr<std::thread> m_pluginEventThread;
    std::mutex m_pluginEventGenerationLoopMutex;
    std::condition_variable m_pluginEventGenerationLoopCondition;
    bool m_terminated = false;

    std::unique_ptr<std::thread> m_eventThread;
    std::condition_variable m_eventGenerationLoopCondition;
    std::mutex m_eventGenerationLoopMutex;
    std::atomic<bool> m_stopping{false};

    bool m_previewAttributesGenerated = false;
    int m_frameCounter = 0;
    int m_objectCounter = 0;
    int m_currentObjectIndex = -1;
    std::vector<nx::sdk::Uuid> m_objectIds;
    std::string m_eventTypeId;
    std::string m_objectTypeId;
    int m_currentObjectTypeIndex = 0;
    int64_t m_lastVideoFrameTimestampUs = 0;
};

const std::string kLineCrossingEventType = "nx.stub.lineCrossing";
const std::string kObjectInTheAreaEventType = "nx.stub.objectInTheArea";
const std::string kLoiteringEventType = "nx.stub.loitering";
const std::string kIntrusionEventType = "nx.stub.intrusion";
const std::string kGunshotEventType = "nx.stub.gunshot";
const std::string kSuspiciousNoiseEventType = "nx.stub.suspiciousNoise";
const std::string kSoundRelatedEventGroup = "nx.stub.soundRelatedEvent";
const std::string kCarObjectType = "nx.stub.car";
const std::string kHumanFaceObjectType = "nx.stub.humanFace";
const std::string kTruckObjectType = "nx.stub.truck";
const std::string kPedestrianObjectType = "nx.stub.pedestrian";
const std::string kBicycleObjectType = "nx.stub.bicycle";

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
