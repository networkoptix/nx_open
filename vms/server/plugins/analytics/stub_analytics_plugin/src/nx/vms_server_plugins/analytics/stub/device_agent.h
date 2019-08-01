// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <thread>
#include <atomic>
#include <memory>
#include <condition_variable>
#include <vector>
#include <string>

#include <nx/sdk/analytics/helpers/video_frame_processing_device_agent.h>
#include <nx/sdk/analytics/helpers/pixel_format.h>
#include <nx/sdk/analytics/helpers/result_aliases.h>

#include "engine.h"
#include "objects/random.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

class DeviceAgent: public nx::sdk::analytics::VideoFrameProcessingDeviceAgent
{
public:
    DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo);
    virtual ~DeviceAgent() override;

    virtual nx::sdk::Result<void> setNeededMetadataTypes(
        const nx::sdk::analytics::IMetadataTypes* neededMetadataTypes) override;

    virtual nx::sdk::SettingsResponseResult pluginSideSettings() const override;

protected:
    virtual std::string manifestString() const override;

    virtual nx::sdk::StringMapResult settingsReceived() override;

    virtual bool pushCompressedVideoFrame(
        const nx::sdk::analytics::ICompressedVideoPacket* videoFrame) override;

    virtual bool pushUncompressedVideoFrame(
        const nx::sdk::analytics::IUncompressedVideoFrame* videoFrame) override;

    virtual bool pullMetadataPackets(
        std::vector<nx::sdk::analytics::IMetadataPacket*>* metadataPackets) override;

private:
    nx::sdk::analytics::IMetadataPacket* cookSomeEvents();
    std::vector<nx::sdk::analytics::IMetadataPacket*> cookSomeObjects();

    int64_t usSinceEpoch() const;

    void processVideoFrame(const nx::sdk::analytics::IDataPacket* videoFrame, const char* func);

    bool checkVideoFrame(const nx::sdk::analytics::IUncompressedVideoFrame* frame) const;

    bool checkVideoFramePlane(
        const nx::sdk::analytics::IUncompressedVideoFrame* frame,
        const nx::sdk::analytics::PixelFormatDescriptor* pixelFormatDescriptor,
        int plane) const;

    void dumpSomeFrameBytes(
        const nx::sdk::analytics::IUncompressedVideoFrame* frame,
        int plane) const;

    void startFetchingMetadata(const nx::sdk::analytics::IMetadataTypes* metadataTypes);

    void stopFetchingMetadata();

    void processEvents();

    void processPluginDiagnosticEvents();

    void setObjectCount(int objectCount);

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

    void updateEventGenerationParameters();

private:
    Engine* const m_engine;

    std::atomic<bool> m_terminated{false};

    std::unique_ptr<std::thread> m_pluginDiagnosticEventThread;
    std::mutex m_pluginDiagnosticEventGenerationLoopMutex;
    std::condition_variable m_pluginDiagnosticEventGenerationLoopCondition;
    std::atomic<bool> m_needToThrowPluginDiagnosticEvents{false};

    std::unique_ptr<std::thread> m_eventThread;
    std::mutex m_eventGenerationLoopMutex;
    std::condition_variable m_eventGenerationLoopCondition;
    std::atomic<bool> m_eventsNeeded{false};

    int m_frameCounter = 0;
    std::string m_eventTypeId;
    int64_t m_lastVideoFrameTimestampUs = 0;

    struct DeviceAgentSettings
    {
        bool needToGenerateObjects()
        {
            return generateCars
                || generateTrucks
                || generatePedestrians
                || generateHumanFaces
                || generateBicycles;
        }

        std::atomic<bool> generateEvents{true};

        std::atomic<bool> generateCars{true};
        std::atomic<bool> generateTrucks{true};
        std::atomic<bool> generatePedestrians{true};
        std::atomic<bool> generateHumanFaces{true};
        std::atomic<bool> generateBicycles{true};

        std::atomic<int> generateObjectsEveryNFrames{1};
        std::atomic<int> numberOfObjectsToGenerate{1};

        std::atomic<bool> generatePreviews{true};

        std::atomic<bool> throwPluginDiagnosticEvents{false};
        std::atomic<bool> leakFrames{false};
    };

    DeviceAgentSettings m_deviceAgentSettings;

    struct EventContext
    {
        int currentEventTypeIndex = 0;
        bool isCurrentEventActive = false;
    };

    EventContext m_eventContext;

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
};

const std::string kLineCrossingEventType = "nx.stub.lineCrossing";
const std::string kObjectInTheAreaEventType = "nx.stub.objectInTheArea";
const std::string kLoiteringEventType = "nx.stub.loitering";
const std::string kIntrusionEventType = "nx.stub.intrusion";
const std::string kGunshotEventType = "nx.stub.gunshot";
const std::string kSuspiciousNoiseEventType = "nx.stub.suspiciousNoise";
const std::string kSoundRelatedEventGroup = "nx.stub.soundRelatedEvent";

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
