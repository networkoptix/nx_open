// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <vector>
#include <string>
#include <mutex>
#include <chrono>
#include <ctime>
#include <thread>
#include <type_traits>

#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>
#include <nx/kit/utils.h>

#include <nx/sdk/analytics/i_motion_metadata_packet.h>

#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_track_best_shot_packet.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/helpers/settings_response.h>
#include <nx/sdk/helpers/error.h>
#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/sdk/helpers/settings_response.h>

#include "utils.h"
#include "settings_model.h"
#include "stub_analytics_plugin_ini.h"

#include "objects/bicycle.h"
#include "objects/vehicles.h"
#include "objects/pedestrian.h"
#include "objects/human_face.h"
#include "objects/stone.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using namespace std::chrono;
using namespace std::literals::chrono_literals;

namespace {

enum class EventContinuityType
{
    impulse,
    prolonged,
};

struct EventDescriptor
{
    std::string eventTypeId;
    std::string caption;
    std::string description;
    EventContinuityType continuityType = EventContinuityType::impulse;

    EventDescriptor(
        std::string eventTypeId,
        std::string caption,
        std::string description,
        EventContinuityType continuityType)
        :
        eventTypeId(std::move(eventTypeId)),
        caption(std::move(caption)),
        description(std::move(description)),
        continuityType(continuityType)
    {
    }
};

static const std::vector<EventDescriptor> kEventsToFire = {
    {
        kObjectInTheAreaEventType,
        "Object in the Area - prolonged event (caption)",
        "Object in the Area - prolonged event (description)",
        EventContinuityType::prolonged
    },
    {
        kLineCrossingEventType,
        "Line crossing - impulse event (caption)",
        "Line crossing - impulse event (description)",
        EventContinuityType::impulse
    },
    {
        kSuspiciousNoiseEventType,
        "Suspicious noise - group impulse event (caption)",
        "Suspicious noise - group impulse event (description)",
        EventContinuityType::impulse
    },
    {
        kGunshotEventType,
        "Gunshot - group impulse event (caption)",
        "Gunshot - group impulse event (description)",
        EventContinuityType::impulse
    }
};

} // namespace

DeviceAgent::DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo):
    ConsumingDeviceAgent(deviceInfo, NX_DEBUG_ENABLE_OUTPUT),
    m_engine(engine)
{
    m_pluginDiagnosticEventThread =
        std::make_unique<std::thread>([this]() { processPluginDiagnosticEvents(); });

    m_eventThread = std::make_unique<std::thread>([this]() { processEvents(); });
}

DeviceAgent::~DeviceAgent()
{
    m_terminated = true;
    {
        std::unique_lock<std::mutex> lock(m_pluginDiagnosticEventGenerationLoopMutex);
        m_pluginDiagnosticEventGenerationLoopCondition.notify_all();
    }

    {
        std::unique_lock<std::mutex> lock(m_eventGenerationLoopMutex);
        m_eventGenerationLoopCondition.notify_all();
    }

    if (m_pluginDiagnosticEventThread)
        m_pluginDiagnosticEventThread->join();

    if (m_eventThread)
        m_eventThread->join();
}

/**
 * DeviceAgent manifest may declare eventTypes and objectTypes similarly to how an Engine declares
 * them - semantically the set from the Engine manifest is joined with the set from the DeviceAgent
 * manifest. Also this manifest should declare supportedEventTypeIds and supportedObjectTypeIds
 * lists which are treated as white-list filters for the respective set from Engine manifest
 * (absent lists are treated as empty lists, thus, disabling all types from the Engine).
 */
std::string DeviceAgent::manifestString() const
{
    static const std::string kAdditionalEventTypes = R"json(,
        {
            "id": "nx.stub.additionalEvent1",
            "name": "Additional event 1"
        },
        {
             "id": "nx.stub.additionalEvent2",
             "name": "Additional event 2"
        })json";

    return /*suppress newline*/ 1 + (const char*) R"json(
{
    "capabilities": ")json" + capabilities() + R"json(",
    "supportedEventTypeIds": [
        ")json" + kLineCrossingEventType + R"json(",
        ")json" + kSuspiciousNoiseEventType + R"json(",
        ")json" + kObjectInTheAreaEventType + R"json("
    ],
    "supportedObjectTypeIds": [
        ")json" + kCarObjectType + R"json(",
        ")json" + kStoneObjectType + R"json("
    ],
    "eventTypes": [
        {
            "id": ")json" + kLoiteringEventType + R"json(",
            "name": "Loitering"
        },
        {
            "id": ")json" + kIntrusionEventType + R"json(",
            "name": "Intrusion",
            "flags": "stateDependent|regionDependent"
        },
        {
            "id": ")json" + kGunshotEventType + R"json(",
            "name": "Gunshot",
            "groupId": ")json" + kSoundRelatedEventGroup + R"json("
        })json"
        + (m_deviceAgentSettings.declareAdditionalEventTypes ? kAdditionalEventTypes : "")
    + R"json(
    ],
    "objectTypes": [
        {
            "id": ")json" + kTruckObjectType + R"json(",
            "name": "Truck"
        },
        {
            "id": ")json" + kPedestrianObjectType + R"json(",
            "name": "Pedestrian"
        },
        {
            "id": ")json" + kBicycleObjectType + R"json(",
            "name": "Bicycle"
        },
        {
            "id": ")json" + kBlinkingObjectType + R"json(",
            "name": "Blinking Object"
        },
        {
          "id": ")json" + kCounterObjectType + R"json(",
          "name": "Counter"
        }
    ]
})json";
}

Result<const ISettingsResponse*> DeviceAgent::settingsReceived()
{
    const auto settingsResponse = new sdk::SettingsResponse();

    const std::string settingsModel = settingValue(kSettingsModelSettings);
    if (settingsModel == kAlternativeSettingsModelOption)
    {
        settingsResponse->setModel(kAlternativeSettingsModel);
    }
    else if (settingsModel == kRegularSettingsModelOption)
    {
        const std::string languagePart = (settingValue(kCitySelector) == kGermanOption)
            ? kGermanCitiesPart
            : kEnglishCitiesPart;

        settingsResponse->setModel(kRegularSettingsModelPart1
            + languagePart
            + kRegularSettingsModelPart2);
    }

    parseSettings();
    updateObjectGenerationParameters();
    updateEventGenerationParameters();
    updateManifest();

    return settingsResponse;
}

/** @param func Name of the caller for logging; supply __func__. */
void DeviceAgent::processVideoFrame(const IDataPacket* videoFrame, const char* func)
{
    if (m_deviceAgentSettings.additionalFrameProcessingDelayMs.load()
        > std::chrono::milliseconds::zero())
    {
        std::this_thread::sleep_for(m_deviceAgentSettings.additionalFrameProcessingDelayMs.load());
    }

    NX_OUTPUT << func << "(): timestamp " << videoFrame->timestampUs() << " us;"
        << " frame #" << m_frameCounter;

    if (m_deviceAgentSettings.leakFrames)
    {
        NX_PRINT << "Intentionally creating a memory leak with IDataPacket @"
            << nx::kit::utils::toString(videoFrame);
        videoFrame->addRef();
    }
    if (m_frameCounter == ini().crashDeviceAgentOnFrameN)
    {
        const std::string message = nx::kit::utils::format(
            "ATTENTION: Intentionally crashing the process at frame #%d as per %s",
            m_frameCounter, ini().iniFile());
        NX_PRINT << message;
        nx::kit::debug::intentionallyCrash(message.c_str());
    }

    ++m_frameCounter;

    const int64_t frameTimestamp = videoFrame->timestampUs();
    m_lastVideoFrameTimestampUs = frameTimestamp;
    if (m_frameTimestampUsQueue.empty() || m_frameTimestampUsQueue.back() < frameTimestamp)
        m_frameTimestampUsQueue.push_back(frameTimestamp);
}

void DeviceAgent::processCustomMetadataPacket(
    const nx::sdk::analytics::ICustomMetadataPacket* customMetadataPacket,
    const char* func)
{
    NX_OUTPUT << func << "(): timestamp " << customMetadataPacket->timestampUs() << " us.";
}

bool DeviceAgent::pushCompressedVideoFrame(const ICompressedVideoPacket* videoFrame)
{
    if (m_engine->needUncompressedVideoFrames())
    {
        NX_PRINT << "ERROR: Received compressed video frame, contrary to manifest.";
        return false;
    }

    NX_OUTPUT << "Received compressed video frame, resolution: "
        << videoFrame->width() << "x" << videoFrame->height();

    processVideoFrame(videoFrame, __func__);
    processFrameMotion(videoFrame->metadataList());
    return true;
}

bool DeviceAgent::pushUncompressedVideoFrame(const IUncompressedVideoFrame* videoFrame)
{
    if (!m_engine->needUncompressedVideoFrames())
    {
        NX_PRINT << "ERROR: Received uncompressed video frame, contrary to manifest.";
        return false;
    }

    NX_OUTPUT << "Received uncompressed video frame, resolution: "
        << videoFrame->width() << "x" << videoFrame->height();

    processVideoFrame(videoFrame, __func__);
    processFrameMotion(videoFrame->metadataList());
    return checkVideoFrame(videoFrame);
}

bool DeviceAgent::pushCustomMetadataPacket(
    const nx::sdk::analytics::ICustomMetadataPacket* customMetadataPacket)
{
    if (!ini().needMetadata)
    {
        NX_PRINT << "ERROR: Received a custom metadata packet, contrary to the manifest.";
        return false;
    }

    processCustomMetadataPacket(customMetadataPacket, __func__);
    return true;
}

void DeviceAgent::processFrameMotion(Ptr<IList<IMetadataPacket>> metadataPacketList)
{
    if (!ini().visualizeMotion)
        return;

    cleanUpTimestampQueue();

    if (!metadataPacketList)
        return;

    const int metadataPacketCount = metadataPacketList->count();
    if (metadataPacketCount == 0)
        return;

    for (int i = 0; i < metadataPacketCount; ++i)
    {
        const auto metadataPacket = metadataPacketList->at(i);
        if (!NX_KIT_ASSERT(metadataPacket))
            continue;

        const auto motionPacket = metadataPacket->queryInterface<IMotionMetadataPacket>();
        if (!motionPacket)
            continue;

        auto objectMetadataPacket = makePtr<ObjectMetadataPacket>();
        objectMetadataPacket->setTimestampUs(motionPacket->timestampUs());

        const int columnCount = motionPacket->columnCount();
        const int rowCount = motionPacket->rowCount();

        for (int column = 0; column < columnCount; ++column)
        {
            for (int row = 0; row < rowCount; ++row)
            {
                if (!motionPacket->isMotionAt(column, row))
                    continue;

                const auto objectMetadata = makePtr<ObjectMetadata>();
                objectMetadata->setBoundingBox(Rect(
                    column / (float) columnCount,
                    row / (float) rowCount,
                    1.0F / columnCount,
                    1.0F / rowCount));

                objectMetadata->setTypeId(kMotionVisualizationObjectType);
                objectMetadata->setTrackId(UuidHelper::randomUuid());
                objectMetadata->setConfidence(1.0F);
                objectMetadataPacket->addItem(objectMetadata.get());
            }
        }

        pushMetadataPacket(objectMetadataPacket.releasePtr());
    }
}

std::string DeviceAgent::capabilities() const
{
    return m_engine->capabilities();
}

bool DeviceAgent::pullMetadataPackets(std::vector<IMetadataPacket*>* metadataPackets)
{
    NX_OUTPUT << __func__ << "() BEGIN";

    if (!m_deviceAgentSettings.needToGenerateObjects())
    {
        NX_OUTPUT << __func__ << "() END -> true: no need to generate object metadata packets";
        return true;
    }

    *metadataPackets = cookSomeObjects();
    m_lastVideoFrameTimestampUs = 0;

    NX_OUTPUT << __func__ << "() END -> true: " <<
        nx::kit::utils::format("generated %d metadata packet(s)", metadataPackets->size());
    return true;
}

void DeviceAgent::doSetNeededMetadataTypes(
    Result<void>* /*outResult*/, const IMetadataTypes* neededMetadataTypes)
{
    if (neededMetadataTypes->isEmpty())
        stopFetchingMetadata();

    startFetchingMetadata(neededMetadataTypes);
}

void DeviceAgent::startFetchingMetadata(const IMetadataTypes* /*metadataTypes*/)
{
    std::unique_lock<std::mutex> lock(m_eventGenerationLoopMutex);
    NX_OUTPUT << __func__ << "() BEGIN";
    m_eventsNeeded = true;
    m_eventGenerationLoopCondition.notify_all();
    m_eventTypeId = kLineCrossingEventType; //< First event to produce.
    NX_OUTPUT << __func__ << "() END -> noError";
}

void DeviceAgent::stopFetchingMetadata()
{
    std::unique_lock<std::mutex> lock(m_eventGenerationLoopMutex);
    NX_OUTPUT << __func__ << "() BEGIN";
    m_eventsNeeded = false;
    NX_OUTPUT << __func__ << "() END -> noError";
}

void DeviceAgent::processEvents()
{
    while (!m_terminated)
    {
        if (m_deviceAgentSettings.generateEvents && m_eventsNeeded)
            pushMetadataPacket(cookSomeEvents());

        std::unique_lock<std::mutex> lock(m_eventGenerationLoopMutex);
        if (m_terminated)
            break;
        // Sleep until the next event needs to be generated, or the thread is ordered
        // to terminate (hence condition variable instead of sleep()). Return value
        // (whether the timeout has occurred) and spurious wake-ups are ignored.
        static const seconds kEventGenerationPeriod{3};
        m_eventGenerationLoopCondition.wait_for(lock, kEventGenerationPeriod);
    }
}

void DeviceAgent::processPluginDiagnosticEvents()
{
    while (!m_terminated)
    {
        if (m_needToThrowPluginDiagnosticEvents)
        {
            pushPluginDiagnosticEvent(
                IPluginDiagnosticEvent::Level::info,
                "Info message from DeviceAgent",
                "Info message description");

            pushPluginDiagnosticEvent(
                IPluginDiagnosticEvent::Level::warning,
                "Warning message from DeviceAgent",
                "Warning message description");

            pushPluginDiagnosticEvent(
                IPluginDiagnosticEvent::Level::error,
                "Error message from DeviceAgent",
                "Error message description");
        }

        {
            std::unique_lock<std::mutex> lock(m_pluginDiagnosticEventGenerationLoopMutex);
            if (m_terminated)
                break;

            // Sleep until the next event needs to be generated, or the thread is ordered to
            // terminate (hence condition variable instead of sleep()). Return value (whether
            // the timeout has occurred) and spurious wake-ups are ignored.
            static const seconds kPluginDiagnosticEventGenerationPeriod{5};
            m_pluginDiagnosticEventGenerationLoopCondition.wait_for(
                lock, kPluginDiagnosticEventGenerationPeriod);
        }
    }
}

void DeviceAgent::getPluginSideSettings(
    Result<const ISettingsResponse*>* outResult) const
{
    const auto response = new SettingsResponse();
    response->setValue("pluginSideTestSpinBox", "100");
    response->setValue("testPolygon", R"json(
        {
            "figure": {
                "points": [
                    [
                        0.138,
                        0.551
                    ],
                    [
                        0.775,
                        0.429
                    ],
                    [
                        0.748,
                        0.844
                    ]
                ]
            }
        })json");

    *outResult = response;
}

//-------------------------------------------------------------------------------------------------
// private

static Ptr<IObjectMetadata> makeObjectMetadata(const AbstractObject* object)
{
    auto objectMetadata = makePtr<ObjectMetadata>();
    objectMetadata->setTypeId(object->typeId());
    objectMetadata->setTrackId(object->id());
    const auto position = object->position();
    const auto size = object->size();
    objectMetadata->setBoundingBox(Rect(position.x, position.y, size.width, size.height));
    objectMetadata->addAttributes(object->attributes());
    return objectMetadata;
}

void DeviceAgent::setObjectCount(int objectCount)
{
    std::unique_lock<std::mutex> lock(m_objectGenerationMutex);

    if (objectCount < 1)
    {
        NX_OUTPUT << "Invalid value for objectCount: " << objectCount << ", assuming 1";
        objectCount = 1;
    }
    m_objectContexts.resize(objectCount);
}

void DeviceAgent::cleanUpTimestampQueue()
{
    std::unique_lock<std::mutex> lock(m_objectGenerationMutex);
    m_frameTimestampUsQueue.clear();
    m_lastVideoFrameTimestampUs = 0;
}

IMetadataPacket* DeviceAgent::cookSomeEvents()
{
    const auto descriptor = kEventsToFire[m_eventContext.currentEventTypeIndex];
    auto eventMetadataPacket = new EventMetadataPacket();
    eventMetadataPacket->setTimestampUs(usSinceEpoch());
    eventMetadataPacket->setDurationUs(0);

    auto eventMetadata = makePtr<EventMetadata>();
    eventMetadata->setTypeId(descriptor.eventTypeId);

    auto nextEventTypeIndex =
        [this]()
        {
            return (m_eventContext.currentEventTypeIndex == (int) kEventsToFire.size() - 1)
                ? 0
                : (m_eventContext.currentEventTypeIndex + 1);
        };

    bool isActive = false;
    auto caption = descriptor.caption;
    auto description = descriptor.description;

    if (descriptor.continuityType == EventContinuityType::prolonged)
    {
        static const std::string kStartedSuffix{" STARTED"};
        static const std::string kFinishedSuffix{" FINISHED"};

        isActive = !m_eventContext.isCurrentEventActive;
        caption += isActive ? kStartedSuffix : kFinishedSuffix;
        description += isActive ? kStartedSuffix : kFinishedSuffix;

        eventMetadata->setIsActive(isActive);
        if (m_eventContext.isCurrentEventActive)
            m_eventContext.currentEventTypeIndex = nextEventTypeIndex();

        m_eventContext.isCurrentEventActive = isActive;
    }
    else
    {
        isActive = true;
        eventMetadata->setIsActive(isActive);
        m_eventContext.isCurrentEventActive = false;
        m_eventContext.currentEventTypeIndex = nextEventTypeIndex();
    }

    eventMetadata->setCaption(std::move(caption));
    eventMetadata->setDescription(std::move(description));

    NX_OUTPUT << "Firing event: "
        << "type: " << descriptor.eventTypeId
        << ", isActive: " << (isActive ? "true" : "false");

    eventMetadataPacket->addItem(eventMetadata.get());
    return eventMetadataPacket;
}

Ptr<IObjectMetadata> DeviceAgent::cookBlinkingObjectIfNeeded(int64_t metadataTimestampUs)
{
    const int64_t blinkingObjectPeriodUs =
        m_deviceAgentSettings.blinkingObjectPeriodMs.load().count() * 1000;

    if (blinkingObjectPeriodUs == 0)
        return nullptr;

    if (m_lastBlinkingObjectTimestampUs != 0 //< Not first time.
        && metadataTimestampUs - m_lastBlinkingObjectTimestampUs < blinkingObjectPeriodUs)
    {
        return nullptr;
    }

    m_lastBlinkingObjectTimestampUs = metadataTimestampUs;

    auto objectMetadata = makePtr<ObjectMetadata>();
    static const Uuid trackId = UuidHelper::randomUuid();
    objectMetadata->setTypeId(kBlinkingObjectType);
    objectMetadata->setTrackId(trackId);
    objectMetadata->setBoundingBox(Rect(0.25, 0.25, 0.5, 0.5));

    return objectMetadata;
}

/**
 * Cooks an object and either packs it into a new metadata packet inserting it into the given list,
 * or adds it to the existing metadata packet - depending on the settings. This is needed to test
 * the ability of the Server to receive multiple metadata packets.
 */
void DeviceAgent::addBlinkingObjectIfNeeded(
    int64_t metadataTimestampUs,
    std::vector<IMetadataPacket*>* metadataPackets,
    Ptr<ObjectMetadataPacket> objectMetadataPacket)
{
    const auto blinkingObjectMetadata = cookBlinkingObjectIfNeeded(metadataTimestampUs);
    if (!blinkingObjectMetadata)
        return;

    if (m_deviceAgentSettings.blinkingObjectInDedicatedPacket)
    {
        if (!NX_KIT_ASSERT(metadataPackets))
            return;

        auto dedicatedMetadataPacket = makePtr<ObjectMetadataPacket>();
        dedicatedMetadataPacket->setTimestampUs(metadataTimestampUs);
        dedicatedMetadataPacket->setDurationUs(0);

        dedicatedMetadataPacket->addItem(blinkingObjectMetadata.get());
        metadataPackets->push_back(dedicatedMetadataPacket.releasePtr());
    }
    else
    {
        objectMetadataPacket->addItem(blinkingObjectMetadata.get());
    }
}

void DeviceAgent::addFixedObjectIfNeeded(Ptr<ObjectMetadataPacket> objectMetadataPacket)
{
    if (!m_deviceAgentSettings.generateFixedObject)
        return;

    auto objectMetadata = makePtr<ObjectMetadata>();
    static const Uuid trackId = UuidHelper::randomUuid();
    objectMetadata->setTypeId(kFixedObjectType);
    objectMetadata->setTrackId(trackId);
    objectMetadata->setBoundingBox(Rect(0.1F, 0.1F, 0.25F, 0.25F));

    objectMetadataPacket->addItem(objectMetadata.get());
}

void DeviceAgent::addCounterIfNeeded(Ptr<ObjectMetadataPacket> objectMetadataPacket)
{
    if (!m_deviceAgentSettings.generateCounter)
        return;

    auto objectMetadata = makePtr<ObjectMetadata>();
    static const Uuid trackId = UuidHelper::randomUuid();
    objectMetadata->setTypeId(kCounterObjectType);
    objectMetadata->setTrackId(trackId);

    const float realXOffset =
        clamp(m_deviceAgentSettings.counterBoundingBoxXOffset.load(), 0.0F, 1.0F);

    const float realYOffset =
        clamp(m_deviceAgentSettings.counterBoundingBoxYOffset.load(), 0.0F, 1.0F);

    const float realWidth =
        clamp(m_deviceAgentSettings.counterBoundingBoxSideSize.load(), 0.0F, 1.0F - realXOffset);

    const float realHeight =
        clamp(m_deviceAgentSettings.counterBoundingBoxSideSize.load(), 0.0F, 1.0F - realYOffset);

    objectMetadata->setBoundingBox(Rect(realXOffset, realYOffset, realWidth, realHeight));
    objectMetadata->addAttribute(makePtr<Attribute>(
        IAttribute::Type::number,
        "counterValue",
        std::to_string(m_counterObjectAttributeValue++)));

    objectMetadataPacket->addItem(objectMetadata.get());
}

std::vector<IMetadataPacket*> DeviceAgent::cookSomeObjects()
{
    std::unique_lock<std::mutex> lock(m_objectGenerationMutex);

    std::vector<IMetadataPacket*> result;

    if (m_lastVideoFrameTimestampUs == 0)
        return result;

    if (m_frameTimestampUsQueue.empty())
        return result;

    const auto metadataTimestampUs = m_frameTimestampUsQueue.front();

    auto objectMetadataPacket = makePtr<ObjectMetadataPacket>();
    objectMetadataPacket->setTimestampUs(metadataTimestampUs);
    objectMetadataPacket->setDurationUs(0);

    addBlinkingObjectIfNeeded(metadataTimestampUs, &result, objectMetadataPacket);
    addFixedObjectIfNeeded(objectMetadataPacket);
    addCounterIfNeeded(objectMetadataPacket);

    const microseconds delay(m_lastVideoFrameTimestampUs - metadataTimestampUs);

    if (delay < m_deviceAgentSettings.overallMetadataDelayMs.load())
        return result;

    m_frameTimestampUsQueue.pop_front();

    if (m_frameCounter % m_deviceAgentSettings.generateObjectsEveryNFrames != 0)
        return result;

    for (auto& context: m_objectContexts)
    {
        if (!context)
            context = m_objectGenerator.generate();

        if (!context)
            continue;

        const auto& object = context.object;
        object->update();

        if (!object->inBounds())
            context.reset();

        if (!object)
            continue;

        ++context.frameCounter;

        const bool previewIsNeeded = m_deviceAgentSettings.generatePreviews
            && context.frameCounter > m_deviceAgentSettings.numberOfFramesBeforePreviewGeneration
            && !context.isPreviewGenerated;

        if (previewIsNeeded)
        {
            const auto position = object->position();
            const auto size = object->size();
            result.push_back(new ObjectTrackBestShotPacket(
                object->id(),
                metadataTimestampUs,
                Rect(position.x, position.y, size.width, size.height)));
            context.isPreviewGenerated = true;
        }

        const auto objectMetadata = makeObjectMetadata(object.get());
        objectMetadataPacket->addItem(objectMetadata.get());
    }

    result.push_back(objectMetadataPacket.releasePtr());
    return result;
}

int64_t DeviceAgent::usSinceEpoch() const
{
    return duration_cast<microseconds>(
        system_clock::now().time_since_epoch()).count();
}

bool DeviceAgent::checkVideoFrame(const IUncompressedVideoFrame* frame) const
{
    if (frame->pixelFormat() != m_engine->pixelFormat())
    {
        NX_PRINT << __func__ << "() ERROR: Video frame has pixel format "
            << pixelFormatToStdString(frame->pixelFormat())
            << " instead of "
            << pixelFormatToStdString(m_engine->pixelFormat());
        return false;
    }

    const auto* const pixelFormatDescriptor = getPixelFormatDescriptor(frame->pixelFormat());
    if (!pixelFormatDescriptor)
        return false; //< Error is already logged.

    NX_KIT_ASSERT(pixelFormatDescriptor->planeCount > 0,
        nx::kit::utils::format("%d", pixelFormatDescriptor->planeCount));

    if (frame->planeCount() != pixelFormatDescriptor->planeCount)
    {
        NX_PRINT << __func__ << "() ERROR: planeCount() is "
            << frame->planeCount() << " instead of " << pixelFormatDescriptor->planeCount;
        return false;
    }

    if (frame->height() % 2 != 0)
    {
        NX_PRINT << __func__ << "() ERROR: height() is not even: " << frame->height();
        return false;
    }

    bool success = true;
    for (int plane = 0; plane < frame->planeCount(); ++plane)
    {
        if (checkVideoFramePlane(frame, pixelFormatDescriptor, plane))
        {
            if (NX_DEBUG_ENABLE_OUTPUT)
                dumpSomeFrameBytes(frame, plane);
        }
        else
        {
            success = false;
        }
    }

    return success;
}

bool DeviceAgent::checkVideoFramePlane(
    const IUncompressedVideoFrame* frame,
    const PixelFormatDescriptor* pixelFormatDescriptor,
    int plane) const
{
    bool success = true;
    if (!frame->data(plane))
    {
        NX_PRINT << __func__ << "() ERROR: data(/*plane*/ " << plane << ") is null";
        success = false;
    }

    if (frame->lineSize(plane) <= 0)
    {
        NX_PRINT << __func__ << "() ERROR: lineSize(/*plane*/ " << plane << ") is "
            << frame->lineSize(plane);
        success = false;
    }

    if (frame->dataSize(plane) <= 0)
    {
        NX_PRINT << __func__ << "() ERROR: dataSize(/*plane*/ " << plane << ") is "
            << frame->dataSize(plane);
        success = false;
    }

    if (!success)
        return false;

    const int bytesPerPlane = (plane == 0)
        ? (frame->height() * frame->lineSize(plane))
        : ((frame->height() / pixelFormatDescriptor->chromaHeightFactor)
            * frame->lineSize(plane));

    if (frame->dataSize(plane) != bytesPerPlane)
    {
        NX_PRINT << __func__ << "() ERROR: dataSize(/*plane*/ " << plane << ") is "
            << frame->dataSize(plane) << " instead of " << bytesPerPlane
            << ", while lineSize(/*plane*/ " << plane << ") is " << frame->lineSize(plane)
            << " and height is " << frame->height();
        return false;
    }

    return true;
}

void DeviceAgent::dumpSomeFrameBytes(
    const nx::sdk::analytics::IUncompressedVideoFrame* frame, int plane) const
{
    // Hex-dump some bytes from raw pixel data.

    static const int dumpOffset = 0;
    static const int dumpSize = 12;

    if (frame->dataSize(plane) < dumpOffset + dumpSize)
    {
        NX_PRINT << __func__ << "(): WARNING: dataSize(/*plane*/ " << plane << ") is "
            << frame->dataSize(plane) << ", which is suspiciously low";
    }
    else
    {
        NX_PRINT_HEX_DUMP(
            nx::kit::utils::format("Plane %d bytes %d..%d of %d",
                plane, dumpOffset, dumpOffset + dumpSize - 1, frame->dataSize(plane)).c_str(),
            frame->data(plane) + dumpOffset, dumpSize);
    }
}

void DeviceAgent::parseSettings()
{
    auto assignNumericSetting =
        [this](
            const std::string& parameterName,
            auto target,
            std::function<void()> onChange = nullptr)
        {
            using namespace nx::kit::utils;
            using SettingType =
                std::conditional_t<
                    std::is_floating_point<decltype(target->load())>::value, float, int>;

            SettingType result{};
            const auto parameterValueString = settingValue(parameterName);
            if (fromString(parameterValueString, &result))
            {
                using AtomicValueType = decltype(target->load());
                if (target->load() != AtomicValueType{result})
                {
                    target->store(AtomicValueType{ result });
                    if (onChange)
                        onChange();
                }
            }
            else
            {
                NX_PRINT << "Received an incorrect setting value for '"
                    << parameterName << "': "
                    << nx::kit::utils::toString(parameterValueString)
                    << ". Expected an integer.";
            }
        };

    m_deviceAgentSettings.generateEvents = toBool(settingValue(kGenerateEventsSetting));
    m_deviceAgentSettings.generateCars = toBool(settingValue(kGenerateCarsSetting));
    m_deviceAgentSettings.generateTrucks = toBool(settingValue(kGenerateTrucksSetting));
    m_deviceAgentSettings.generatePedestrians = toBool(settingValue(kGeneratePedestriansSetting));
    m_deviceAgentSettings.generateHumanFaces = toBool(settingValue(kGenerateHumanFacesSetting));
    m_deviceAgentSettings.generateBicycles = toBool(settingValue(kGenerateBicyclesSetting));
    m_deviceAgentSettings.generateStones = toBool(settingValue(kGenerateStonesSetting));
    m_deviceAgentSettings.generateFixedObject = toBool(settingValue(kGenerateFixedObjectSetting));
    m_deviceAgentSettings.generateCounter = toBool(settingValue(kGenerateCounterSetting));
    m_deviceAgentSettings.declareAdditionalEventTypes =
        toBool(settingValue(kDeclareAdditionalEventTypesSetting));

    assignNumericSetting(kBlinkingObjectPeriodMsSetting,
        &m_deviceAgentSettings.blinkingObjectPeriodMs);

    m_deviceAgentSettings.blinkingObjectInDedicatedPacket =
        toBool(settingValue(kBlinkingObjectInDedicatedPacketSetting));

    m_deviceAgentSettings.generatePreviews = toBool(settingValue(kGeneratePreviewPacketSetting));
    m_deviceAgentSettings.throwPluginDiagnosticEvents = toBool(
        settingValue(kThrowPluginDiagnosticEventsFromDeviceAgentSetting));
    m_deviceAgentSettings.leakFrames = toBool(settingValue(kLeakFramesSetting));

    assignNumericSetting(
        kGenerateObjectsEveryNFramesSetting,
        &m_deviceAgentSettings.generateObjectsEveryNFrames);

    assignNumericSetting(
        kNumberOfObjectsToGenerateSetting,
        &m_deviceAgentSettings.numberOfObjectsToGenerate);

    assignNumericSetting(
        kAdditionalFrameProcessingDelayMsSetting,
        &m_deviceAgentSettings.additionalFrameProcessingDelayMs);

    assignNumericSetting(
        kOverallMetadataDelayMsSetting,
        &m_deviceAgentSettings.overallMetadataDelayMs,
        [this]() { cleanUpTimestampQueue(); });

    assignNumericSetting(
        kGeneratePreviewAfterNFramesSetting,
        &m_deviceAgentSettings.numberOfFramesBeforePreviewGeneration);

    assignNumericSetting(
        kCounterBoundingBoxSideSizeSetting,
        &m_deviceAgentSettings.counterBoundingBoxSideSize);

    assignNumericSetting(
        kCounterXOffsetSetting,
        &m_deviceAgentSettings.counterBoundingBoxXOffset);

    assignNumericSetting(
        kCounterYOffsetSetting,
        &m_deviceAgentSettings.counterBoundingBoxYOffset);
}

void DeviceAgent::updateObjectGenerationParameters()
{
    setObjectCount(m_deviceAgentSettings.numberOfObjectsToGenerate);
    setIsObjectTypeGenerationNeeded<Car>(m_deviceAgentSettings.generateCars);
    setIsObjectTypeGenerationNeeded<Truck>(m_deviceAgentSettings.generateTrucks);
    setIsObjectTypeGenerationNeeded<Pedestrian>(m_deviceAgentSettings.generatePedestrians);
    setIsObjectTypeGenerationNeeded<HumanFace>(m_deviceAgentSettings.generateHumanFaces);
    setIsObjectTypeGenerationNeeded<Bicycle>(m_deviceAgentSettings.generateBicycles);
    setIsObjectTypeGenerationNeeded<Stone>(m_deviceAgentSettings.generateStones);
}

void DeviceAgent::updateEventGenerationParameters()
{
    if (m_deviceAgentSettings.throwPluginDiagnosticEvents)
    {
        NX_PRINT << __func__ << "(): Starting Plugin Diagnostic Events generation.";
        m_needToThrowPluginDiagnosticEvents = true;
    }
    else
    {
        NX_PRINT << __func__ << "(): Stopping Plugin Diagnostic Events generation.";
        m_needToThrowPluginDiagnosticEvents = false;
        m_pluginDiagnosticEventGenerationLoopCondition.notify_all();
    }
}

void DeviceAgent::updateManifest()
{
    pushManifest(manifestString());
}

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
