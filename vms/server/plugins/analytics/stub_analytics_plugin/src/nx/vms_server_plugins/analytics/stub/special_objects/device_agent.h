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
#include "stub_analytics_plugin_special_objects_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace special_objects {

const std::string kBlinkingObjectType = "nx.stub.blinkingObject";
const std::string kFixedObjectType = "nx.stub.fixedObject";
const std::string kCounterObjectType = "nx.stub.counter";

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

    virtual bool pullMetadataPackets(
        std::vector<nx::sdk::analytics::IMetadataPacket*>* metadataPackets) override;

private:
    std::vector<nx::sdk::analytics::IMetadataPacket*> cookSomeObjects();

    nx::sdk::Ptr<nx::sdk::analytics::IObjectMetadata> cookBlinkingObjectIfNeeded(
        int64_t metadataTimestampUs);

    void addBlinkingObjectIfNeeded(
        int64_t metadataTimestampUs,
        std::vector<nx::sdk::analytics::IMetadataPacket*>* metadataPackets,
        nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadataPacket> objectMetadataPacket);

    void addFixedObjectIfNeeded(
        nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadataPacket> objectMetadataPacket);

    void addCounterIfNeeded(
        nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadataPacket> objectMetadataPacket);

    void processVideoFrame(const nx::sdk::analytics::IDataPacket* videoFrame, const char* func);

    void processCustomMetadataPacket(
        const nx::sdk::analytics::ICustomMetadataPacket* customMetadataPacket,
        const char* func);

    void cleanUpTimestampQueue();

private:
    Engine* const m_engine;

    int m_frameCounter = 0;

    std::deque<int64_t> m_frameTimestampUsQueue;
    int64_t m_lastVideoFrameTimestampUs = 0;
    int64_t m_lastBlinkingObjectTimestampUs = 0;

    struct DeviceAgentSettings
    {
        bool needToGenerateObjects() const
        {
            return generateFixedObject
                || generateCounter
                || blinkingObjectPeriodMs.load() != std::chrono::milliseconds::zero();
        }

        std::atomic<bool> generateFixedObject{false};

        std::mutex fixedObjectColorMutex;
        std::string fixedObjectColor;

        std::atomic<bool> generateCounter{false};

        std::atomic<std::chrono::milliseconds> blinkingObjectPeriodMs{
            std::chrono::milliseconds::zero()};

        std::atomic<bool> blinkingObjectInDedicatedPacket{false};

        std::atomic<int> generateObjectsEveryNFrames{1};

        std::atomic<std::chrono::milliseconds> additionalFrameProcessingDelayMs{
            std::chrono::milliseconds::zero()};

        std::atomic<std::chrono::milliseconds> overallMetadataDelayMs{
            std::chrono::milliseconds::zero()};

        std::atomic<float> counterBoundingBoxSideSize{0};
        std::atomic<float> counterBoundingBoxXOffset{0};
        std::atomic<float> counterBoundingBoxYOffset{0};
    };

    DeviceAgentSettings m_deviceAgentSettings;

    std::mutex m_objectGenerationMutex;
    int m_counterObjectAttributeValue = 0;
};

} // namespace special_objects
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
