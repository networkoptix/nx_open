// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <iostream>
#include <chrono>
#include <ctime>
#include <cmath>

#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_track_best_shot_packet.h>
#include <nx/sdk/helpers/string_map.h>

#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>
#include <nx/kit/utils.h>

#include "utils.h"
#include "stub_analytics_plugin_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

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
    VideoFrameProcessingDeviceAgent(engine, deviceInfo, NX_DEBUG_ENABLE_OUTPUT)
{
    generateObjectIds();
}

DeviceAgent::~DeviceAgent()
{
    {
        std::unique_lock<std::mutex> lock(m_pluginEventGenerationLoopMutex);
        m_terminated = true;
        m_needToThrowPluginEvents = false;
    }
    m_pluginEventGenerationLoopCondition.notify_all();
    if (m_pluginEventThread)
        m_pluginEventThread->join();
}

/**
 * DeviceAgent manifest may declare eventTypes and objectTypes similarly to how an Engine declares
 * them - semantically the set from the Engine manifest is joined with the set from the DeviceAgent
 * manifest. Also this manifest should declare supportedEventTypeIds and supportedObjectTypeIds
 * lists which are treated as white-list filters for the respective set (absent lists are treated
 * as empty lists, thus, disabling all types from the Engine).
 */
std::string DeviceAgent::manifest() const
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

void DeviceAgent::settingsReceived()
{
    parseSettings();

    if (m_deviceAgentSettings.throwPluginEvents && !m_pluginEventThread)
    {
        NX_PRINT << __func__ << "(): Starting plugin event generation thread";
        m_needToThrowPluginEvents = true;
        m_pluginEventThread = std::make_unique<std::thread>([this]() { processPluginEvents(); });
    }
    else if (!m_deviceAgentSettings.throwPluginEvents && m_pluginEventThread)
    {
        NX_PRINT << __func__ << "(): Stopping plugin event generation thread";
        m_needToThrowPluginEvents = false;
        m_pluginEventGenerationLoopCondition.notify_all();
        m_pluginEventThread->join();
        m_pluginEventThread.reset();
    }
}

/** @param func Name of the caller for logging; supply __func__. */
void DeviceAgent::processVideoFrame(const IDataPacket* videoFrame, const char* func)
{
    NX_OUTPUT << func << "(): timestamp " << videoFrame->timestampUs() << " us;"
        << " frame #" << m_frameCounter;

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
    if (engine()->needUncompressedVideoFrames())
    {
        NX_PRINT << "ERROR: Received compressed video frame, contrary to manifest.";
        return false;
    }

    processVideoFrame(videoFrame, __func__);
    return true;
}

bool DeviceAgent::pushUncompressedVideoFrame(const IUncompressedVideoFrame* videoFrame)
{
    if (!engine()->needUncompressedVideoFrames())
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
    if (m_deviceAgentSettings.generateObjects)
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
    else
    {
        logMessage = "Objects generation disabled by .ini";
    }

    m_lastVideoFrameTimestampUs = 0;

    NX_OUTPUT << __func__ << "() END -> true: " << logMessage;
    return true;
}

Error DeviceAgent::setNeededMetadataTypes(const IMetadataTypes* metadataTypes)
{
    if (metadataTypes->isEmpty())
    {
        stopFetchingMetadata();
        return Error::noError;
    }

    return startFetchingMetadata(metadataTypes);
}

Error DeviceAgent::startFetchingMetadata(const IMetadataTypes* /*metadataTypes*/)
{
    NX_OUTPUT << __func__ << "() BEGIN";

    m_eventTypeId = kLineCrossingEventType; //< First event to produce.

    if (m_deviceAgentSettings.generateEvents)
    {
        auto metadataDigger =
            [this]()
            {
                while (!m_stopping)
                {
                    using namespace std::chrono_literals;
                    pushMetadataPacket(cookSomeEvents());
                    std::unique_lock<std::mutex> lock(m_eventGenerationLoopMutex);
                    // Sleep until the next event needs to be generated, or the thread is ordered
                    // to terminate (hence condition variable instead of sleep()). Return value
                    // (whether the timeout has occurred) and spurious wake-ups are ignored.
                    m_eventGenerationLoopCondition.wait_for(lock, 3000ms);
                }
            };

        NX_PRINT << "Starting event generation thread";
        if (!m_eventThread)
            m_eventThread.reset(new std::thread(metadataDigger));
    }

    NX_OUTPUT << __func__ << "() END -> noError";
    return Error::noError;
}

void DeviceAgent::stopFetchingMetadata()
{
    NX_OUTPUT << __func__ << "() BEGIN";
    m_stopping = true;

    // Wake up event generation thread to avoid waiting until its sleeping period expires.
    m_eventGenerationLoopCondition.notify_all();

    if (m_eventThread)
    {
        m_eventThread->join();
        m_eventThread.reset();
    }
    m_stopping = false;

    NX_OUTPUT << __func__ << "() END -> noError";
}

void DeviceAgent::processPluginEvents()
{
    while (!m_terminated && m_needToThrowPluginEvents)
    {
        using namespace std::chrono_literals;

        pushPluginEvent(
            IPluginEvent::Level::info,
            "Info message from DeviceAgent",
            "Info message description");

        pushPluginEvent(
            IPluginEvent::Level::warning,
            "Warning message from DeviceAgent",
            "Warning message description");

        pushPluginEvent(
            IPluginEvent::Level::error,
            "Error message from DeviceAgent",
            "Error message description");

        // Sleep until the next event needs to be generated, or the thread is ordered to
        // terminate (hence condition variable instead of sleep()). Return value (whether
        // the timeout has occurred) and spurious wake-ups are ignored.
        {
            std::unique_lock<std::mutex> lock(m_pluginEventGenerationLoopMutex);
            if (m_terminated || !m_needToThrowPluginEvents)
                break;
            static const std::chrono::seconds kPluginEventGenerationPeriod{5};
            m_pluginEventGenerationLoopCondition.wait_for(lock, kPluginEventGenerationPeriod);
        }
    }
}

IStringMap* DeviceAgent::pluginSideSettings() const
{
    auto settings = new StringMap();
    settings->addItem("plugin_side_number", "100");

    return settings;
}

//-------------------------------------------------------------------------------------------------
// private

static IObjectMetadata* makeObjectMetadata(
    const std::string& objectTypeId,
    const Uuid& objectId,
    double offset,
    int64_t lastVideoFrameTimestampUs,
    int objectIndex)
{
    auto objectMetadata = new ObjectMetadata();
    objectMetadata->setTypeId(objectTypeId);
    objectMetadata->setId(objectId);
    objectMetadata->setBoundingBox(
		Rect((float) offset, (float) offset + 0.05F * (float) objectIndex, 0.25F, 0.25F));

    const std::map<std::string, std::vector<nx::sdk::Ptr<Attribute>>> kObjectAttributes = {
        {kCarObjectType, {
            nx::sdk::makePtr<Attribute>(IAttribute::Type::string, "Brand", "Tesla"),
            nx::sdk::makePtr<Attribute>(IAttribute::Type::string, "Model", "X"),
            nx::sdk::makePtr<Attribute>(IAttribute::Type::string, "Color", "Pink"),
        }},
        {kHumanFaceObjectType, {
            nx::sdk::makePtr<Attribute>(IAttribute::Type::string, "Sex", "Female"),
            nx::sdk::makePtr<Attribute>(IAttribute::Type::string, "Hair color", "Red"),
            nx::sdk::makePtr<Attribute>(IAttribute::Type::string, "Age", "29"),
            nx::sdk::makePtr<Attribute>(IAttribute::Type::string, "Name", "Triss"),

        }},
        {kTruckObjectType, {
            nx::sdk::makePtr<Attribute>(IAttribute::Type::string, "Length", "12 m"),
        }},
        {kPedestrianObjectType, {
            nx::sdk::makePtr<Attribute>(
                IAttribute::Type::string,
                "Direction",
                "Towards the camera"),
            nx::sdk::makePtr<Attribute>(IAttribute::Type::string, "Clothes color", "White"),
        }},
        {kBicycleObjectType, {
            nx::sdk::makePtr<Attribute>(IAttribute::Type::string, "Type", "Mountain bike"),
        }},
    };

    objectMetadata->addAttributes(kObjectAttributes.at(objectTypeId));

    return objectMetadata;
}

static IObjectTrackBestShotPacket* makeObjectTrackBestShotPacket(
    const Uuid& objectTrackId,
    int64_t timestampUs,
    float offset)
{
    return new ObjectTrackBestShotPacket(
        objectTrackId,
        timestampUs,
        Rect(offset, offset, 0.1F, 0.1F));
}

void DeviceAgent::generateObjectIds()
{
    int objectCount = m_deviceAgentSettings.numberOfObjectsToGenerate;
    if (objectCount < 1)
    {
        NX_OUTPUT << "Invalid value for objectCount in .ini; assuming 1.";
        objectCount = 1;
    }
    m_objectIds.resize(objectCount);
    for (auto& objectId: m_objectIds)
        objectId = UuidHelper::randomUuid();
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
            return (m_eventContext.currentEventTypeIndex == kEventsToFire.size() - 1)
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
    std::vector<IMetadataPacket*> result;
    if (m_lastVideoFrameTimestampUs == 0)
        return {};

    if (m_frameCounter % m_deviceAgentSettings.generateObjectsEveryNFrames != 0)
        return {};

    float dt = m_objectCounter / 32.0F;
    ++m_objectCounter;
    double intPart;
    dt = (float) modf(dt, &intPart) * 0.75F;
    const int sequentialNumber = static_cast<int>(intPart);
    static const std::vector<std::string> kObjectTypes = {
        kCarObjectType,
        kHumanFaceObjectType,
        kTruckObjectType,
        kPedestrianObjectType,
        kBicycleObjectType,
    };

    if (m_currentObjectIndex != sequentialNumber)
    {
        generateObjectIds();
        m_currentObjectIndex = sequentialNumber;
        m_objectTypeId = kObjectTypes.at(m_currentObjectTypeIndex);
        ++m_currentObjectTypeIndex;

        if (m_currentObjectTypeIndex == (int) kObjectTypes.size())
            m_currentObjectTypeIndex = 0;
    }

    bool generatePreviewPacket = false;
    if (dt < 0.5)
    {
        m_previewAttributesGenerated = false;
    }
    else if (dt > 0.5
        && !m_previewAttributesGenerated
        && m_deviceAgentSettings.generatePreviewPacket)
    {
        m_previewAttributesGenerated = true;
        generatePreviewPacket = true;
    }

    auto objectMetadataPacket = new ObjectMetadataPacket();

    for (int i = 0; i < (int) m_objectIds.size(); ++i)
    {
        const auto objectMetadata = toPtr(makeObjectMetadata(
            m_objectTypeId,
            m_objectIds[i],
            dt,
            m_lastVideoFrameTimestampUs,
            i));

        objectMetadataPacket->addItem(objectMetadata.get());

        if (generatePreviewPacket)
        {
            if (const auto bestShotMetadataPacket = makeObjectTrackBestShotPacket(
                m_objectIds[i],
                m_lastVideoFrameTimestampUs,
                dt))
            {
                result.push_back(bestShotMetadataPacket);
            }
        }
    }

    objectMetadataPacket->setTimestampUs(m_lastVideoFrameTimestampUs);
    objectMetadataPacket->setDurationUs(0);
    result.push_back(objectMetadataPacket);
    return result;
}

int64_t DeviceAgent::usSinceEpoch() const
{
    using namespace std::chrono;
    return duration_cast<microseconds>(
        system_clock::now().time_since_epoch()).count();
}

bool DeviceAgent::checkVideoFrame(const IUncompressedVideoFrame* frame) const
{
    if (frame->pixelFormat() != engine()->pixelFormat())
    {
        NX_PRINT << __func__ << "() ERROR: Video frame has pixel format "
            << pixelFormatToStdString(frame->pixelFormat())
            << " instead of "
            << pixelFormatToStdString(engine()->pixelFormat());
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
        [this](const std::string& parameterName, std::atomic<int>* target)
        {
            using namespace nx::kit::utils;

            int result = 0;
            const auto parameterValueString = getParamValue(parameterName);
            if (const bool success = fromString(parameterValueString, &result))
            {
                *target = result;
            }
            else
            {
                NX_PRINT << "Received an incorrect setting value for '"
                    << parameterName << "' "
                    << parameterValueString
                    << ". Expected an integer";
            }
        };

    m_deviceAgentSettings.generateEvents = toBool(getParamValue(kGenerateEventsSetting));
    m_deviceAgentSettings.generateObjects = toBool(getParamValue(kGenerateObjectsSetting));
    assignIntegerSetting(
        kGenerateObjectsEveryNFramesSetting,
        &m_deviceAgentSettings.generateObjectsEveryNFrames);

    assignIntegerSetting(
        kNumberOfObjectsToGenerateSetting,
        &m_deviceAgentSettings.numberOfObjectsToGenerate);

    m_deviceAgentSettings.generatePreviewPacket =
        toBool(getParamValue(kGeneratePreviewPacketSetting));
    m_deviceAgentSettings.throwPluginEvents =
        toBool(getParamValue(kThrowPluginEventsFromDeviceAgentSetting));
}

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
