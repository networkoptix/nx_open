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

#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_track_best_shot_packet.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/helpers/settings_response.h>
#include <nx/sdk/helpers/error.h>

#include "utils.h"
#include "stub_analytics_plugin_ini.h"

#include "objects/bicycle.h"
#include "objects/vehicles.h"
#include "objects/pedestrian.h"
#include "objects/human_face.h"

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
    VideoFrameProcessingDeviceAgent(deviceInfo, NX_DEBUG_ENABLE_OUTPUT),
    m_engine(engine)
{
    m_pluginDiagnosticEventThread =
        std::make_unique<std::thread>([this]() { processPluginDiagnosticEvents(); });

    m_eventThread = std::make_unique<std::thread>([this]() { processEvents(); });
}

DeviceAgent::~DeviceAgent()
{
    {
        std::unique_lock<std::mutex> lock(m_pluginDiagnosticEventGenerationLoopMutex);
        m_terminated = true;
        m_pluginDiagnosticEventGenerationLoopCondition.notify_all();
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
 * lists which are treated as white-list filters for the respective set (absent lists are treated
 * as empty lists, thus, disabling all types from the Engine).
 */
std::string DeviceAgent::manifestString() const
{
    return /*suppress newline*/1 + R"json(
{
    "supportedEventTypeIds": [
        ")json" + kLineCrossingEventType + R"json(",
        ")json" + kSuspiciousNoiseEventType + R"json(",
        ")json" + kObjectInTheAreaEventType + R"json("
    ],
    "supportedObjectTypeIds": [
        ")json" + kCarObjectType + R"json("
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
        }
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
        }
    ]
}
)json";
}

Result<const IStringMap*> DeviceAgent::settingsReceived()
{
    parseSettings();
    updateObjectGenerationParameters();
    updateEventGenerationParameters();

    return nullptr;
}

/** @param func Name of the caller for logging; supply __func__. */
void DeviceAgent::processVideoFrame(const IDataPacket* videoFrame, const char* func)
{
    if (m_deviceAgentSettings.additionalFrameProcessingDelay.load()
        > std::chrono::milliseconds::zero())
    {
        std::this_thread::sleep_for(m_deviceAgentSettings.additionalFrameProcessingDelay.load());
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
    m_lastVideoFrameTimestampUs = videoFrame->timestampUs();
}

bool DeviceAgent::pushCompressedVideoFrame(const ICompressedVideoPacket* videoFrame)
{
    if (m_engine->needUncompressedVideoFrames())
    {
        NX_PRINT << "ERROR: Received compressed video frame, contrary to manifest.";
        return false;
    }

    processVideoFrame(videoFrame, __func__);
    return true;
}

bool DeviceAgent::pushUncompressedVideoFrame(const IUncompressedVideoFrame* videoFrame)
{
    if (!m_engine->needUncompressedVideoFrames())
    {
        NX_PRINT << "ERROR: Received uncompressed video frame, contrary to manifest.";
        return false;
    }

    processVideoFrame(videoFrame, __func__);
    return checkVideoFrame(videoFrame);
}

bool DeviceAgent::pullMetadataPackets(std::vector<IMetadataPacket*>* metadataPackets)
{
    NX_OUTPUT << __func__ << "() BEGIN";

    const char* logMessage = "";
    if (m_deviceAgentSettings.needToGenerateObjects())
    {
        const std::vector<IMetadataPacket*> result = cookSomeObjects();
        if (!result.empty())
        {
            *metadataPackets = result;
            logMessage = "Generated 1 metadata packet";
        }
        else
        {
            logMessage = "Generated 0 metadata packets";
        }
    }

    m_lastVideoFrameTimestampUs = 0;

    NX_OUTPUT << __func__ << "() END -> true: " << logMessage;
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
    NX_OUTPUT << __func__ << "() BEGIN";
    m_eventsNeeded = true;
    m_eventGenerationLoopCondition.notify_all();
    m_eventTypeId = kLineCrossingEventType; //< First event to produce.
    NX_OUTPUT << __func__ << "() END -> noError";
}

void DeviceAgent::stopFetchingMetadata()
{
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

    *outResult = response;
}

//-------------------------------------------------------------------------------------------------
// private

static IObjectMetadata* makeObjectMetadata(const AbstractObject* object)
{
    auto objectMetadata = new ObjectMetadata();
    objectMetadata->setTypeId(object->typeId());
    objectMetadata->setTrackId(object->id());
    const auto position = object->position();
    const auto size = object->size();
    objectMetadata->setBoundingBox(Rect(position.x, position.y, size.width, size.height));
    objectMetadata->addAttributes(object->attributes());
    return objectMetadata;
}

static IObjectTrackBestShotPacket* makeObjectTrackBestShotPacket(
    const Uuid& objectTrackId,
    int64_t timestampUs,
    Rect boundingBox)
{
    return new ObjectTrackBestShotPacket(
        objectTrackId,
        timestampUs,
        std::move(boundingBox));
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

std::vector<IMetadataPacket*> DeviceAgent::cookSomeObjects()
{
    std::unique_lock<std::mutex> lock(m_objectGenerationMutex);

    std::vector<IMetadataPacket*> result;
    if (m_lastVideoFrameTimestampUs == 0)
        return {};

    if (m_frameCounter % m_deviceAgentSettings.generateObjectsEveryNFrames != 0)
        return {};

    auto objectMetadataPacket = new ObjectMetadataPacket();
    objectMetadataPacket->setTimestampUs(m_lastVideoFrameTimestampUs);
    objectMetadataPacket->setDurationUs(0);

    for (auto& context: m_objectContexts)
    {
        if (!context)
            context = m_objectGenerator.generate();

        if (!context)
            continue;

        auto& object = context.object;
        object->update();

        if (!object->inBounds())
            context.reset();

        if (!object)
            continue;

        ++(context.frameCounter);

        bool previewIsNeeded = m_deviceAgentSettings.generatePreviews
            && context.frameCounter > m_deviceAgentSettings.numberOfFramesBeforePreviewGeneration
            && !context.isPreviewGenerated;

        if (previewIsNeeded)
        {
            const auto position = object->position();
            const auto size = object->size();
            auto bestShotPacket = makeObjectTrackBestShotPacket(
                object->id(),
                m_lastVideoFrameTimestampUs,
                Rect(position.x, position.y, size.width, size.height));

            if (bestShotPacket)
            {
                result.push_back(bestShotPacket);
                context.isPreviewGenerated = true;
            }
        }

        auto objectMetadata = toPtr(makeObjectMetadata(object.get()));
        objectMetadataPacket->addItem(objectMetadata.get());
    }

    result.push_back(objectMetadataPacket);
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
    auto assignIntegerSetting =
        [this](const std::string& parameterName, auto target)
        {
            using namespace nx::kit::utils;
            int result = 0;
            const auto parameterValueString = settingValue(parameterName);
            if (fromString(parameterValueString, &result))
            {
                using AtomicValueType = decltype(target->load());
                target->store(AtomicValueType{result});
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
    m_deviceAgentSettings.generatePreviews = toBool(settingValue(kGeneratePreviewPacketSetting));
    m_deviceAgentSettings.throwPluginDiagnosticEvents = toBool(
        settingValue(kThrowPluginDiagnosticEventsFromDeviceAgentSetting));
    m_deviceAgentSettings.leakFrames = toBool(settingValue(kLeakFrames));

    assignIntegerSetting(
        kGenerateObjectsEveryNFramesSetting,
        &m_deviceAgentSettings.generateObjectsEveryNFrames);

    assignIntegerSetting(
        kNumberOfObjectsToGenerateSetting,
        &m_deviceAgentSettings.numberOfObjectsToGenerate);

    assignIntegerSetting(
        kAdditionalFrameProcessingDelayMs,
        &m_deviceAgentSettings.additionalFrameProcessingDelay);

    assignIntegerSetting(
        kGeneratePreviewAfterNFramesSetting,
        &m_deviceAgentSettings.numberOfFramesBeforePreviewGeneration);
}

void DeviceAgent::updateObjectGenerationParameters()
{
    setObjectCount(m_deviceAgentSettings.numberOfObjectsToGenerate);
    setIsObjectTypeGenerationNeeded<Car>(m_deviceAgentSettings.generateCars);
    setIsObjectTypeGenerationNeeded<Truck>(m_deviceAgentSettings.generateTrucks);
    setIsObjectTypeGenerationNeeded<Pedestrian>(m_deviceAgentSettings.generatePedestrians);
    setIsObjectTypeGenerationNeeded<HumanFace>(m_deviceAgentSettings.generateHumanFaces);
    setIsObjectTypeGenerationNeeded<Bicycle>(m_deviceAgentSettings.generateBicycles);
}

void DeviceAgent::updateEventGenerationParameters()
{
    if (m_deviceAgentSettings.throwPluginDiagnosticEvents)
    {
        NX_PRINT << __func__ << "(): Starting plugin event generation";
        m_needToThrowPluginDiagnosticEvents = true;
    }
    else
    {
        NX_PRINT << __func__ << "(): Stopping plugin event generation";
        m_needToThrowPluginDiagnosticEvents = false;
        m_pluginDiagnosticEventGenerationLoopCondition.notify_all();
    }
}

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
