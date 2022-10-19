// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <thread>
#include <atomic>
#include <memory>
#include <condition_variable>
#include <vector>
#include <string>
#include <deque>

#include <nx/sdk/analytics/helpers/consuming_device_agent.h>
#include <nx/sdk/analytics/helpers/pixel_format.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>

#include "objects/random.h"
#include "modules/standard_taxonomy/module.h"

#include "engine.h"
#include "stub_analytics_plugin_deprecated_object_detection_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace deprecated_object_detection {

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

    void setObjectCount(int objectCount);

    void cleanUpTimestampQueue();

    void parseSettings();

    template<typename ObjectType>
    void setIsObjectTypeGenerationNeeded(bool isObjectTypeGenerationNeeded)
    {
        if (isObjectTypeGenerationNeeded)
        {
            m_objectGenerator.registerObjectFactory<ObjectType>(
                []() { return std::make_unique<ObjectType>(); });
        }
        else
        {
            m_objectGenerator.unregisterObjectFactory<ObjectType>();
        }
    }

    void updateObjectGenerationParameters();

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
            return generateCars
                || generateTrucks
                || generatePedestrians
                || generateHumanFaces
                || generateBicycles
                || generateStones;
        }

        std::atomic<bool> generateCars{true};
        std::atomic<bool> generateTrucks{true};
        std::atomic<bool> generatePedestrians{true};
        std::atomic<bool> generateHumanFaces{true};
        std::atomic<bool> generateBicycles{true};
        std::atomic<bool> generateStones{false};
        std::atomic<bool> generateFixedObject{false};

        std::mutex fixedObjectColorMutex;
        std::string fixedObjectColor;

        std::atomic<bool> generateCounter{false};

        std::atomic<std::chrono::milliseconds> blinkingObjectPeriodMs{
            std::chrono::milliseconds::zero()};

        std::atomic<bool> blinkingObjectInDedicatedPacket{false};

        std::atomic<int> numberOfObjectsToGenerate{1};
        std::atomic<int> generateObjectsEveryNFrames{1};

        std::atomic<bool> generatePreviews{true};

        std::atomic<std::chrono::milliseconds> additionalFrameProcessingDelayMs{
            std::chrono::milliseconds::zero()};

        std::atomic<std::chrono::milliseconds> overallMetadataDelayMs{
            std::chrono::milliseconds::zero()};

        std::atomic<int> numberOfFramesBeforePreviewGeneration{30};

        std::atomic<float> counterBoundingBoxSideSize{0};
        std::atomic<float> counterBoundingBoxXOffset{0};
        std::atomic<float> counterBoundingBoxYOffset{0};

        std::vector<char> previewImage;
        std::string previewImageFormat;
        std::string previewImageUrl;
    };

    DeviceAgentSettings m_deviceAgentSettings;

    struct ObjectContext
    {
        ObjectContext() = default;
        ObjectContext(std::unique_ptr<AbstractObject> object): object(std::move(object)) {}

        ObjectContext& operator=(std::unique_ptr<AbstractObject>&& otherObject)
        {
            reset();
            object = std::move(otherObject);
            return *this;
        }

        void reset()
        {
            object.reset();
            isPreviewGenerated = false;
            frameCounter = 0;
        }

        bool operator!() const { return !object; }

        std::unique_ptr<AbstractObject> object;
        bool isPreviewGenerated = false;
        int frameCounter = 0;
    };

    std::mutex m_objectGenerationMutex;
    RandomObjectGenerator m_objectGenerator;
    std::vector<ObjectContext> m_objectContexts;
    int m_counterObjectAttributeValue = 0;

    modules::standard_taxonomy::Module m_standardTaxonomyModule;
};

const std::string kBlinkingObjectType = "nx.stub.blinkingObject";
const std::string kFixedObjectType = "nx.stub.fixedObject";
const std::string kCounterObjectType = "nx.stub.counter";

} // namespace deprecated_object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
