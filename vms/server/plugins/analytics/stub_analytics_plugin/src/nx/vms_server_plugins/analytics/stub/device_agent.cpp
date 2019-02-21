#include "device_agent.h"

#include <iostream>
#include <chrono>
#include <ctime>
#include <cmath>
#include <vector>

#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/helpers/string_map.h>

#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

#include "stub_analytics_plugin_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

DeviceAgent::DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo):
    VideoFrameProcessingDeviceAgent(engine, deviceInfo, NX_DEBUG_ENABLE_OUTPUT),
    m_objectId(nx::sdk::UuidHelper::randomUuid())
{
}

DeviceAgent::~DeviceAgent()
{
    {
        std::unique_lock<std::mutex> lock(m_pluginEventGenerationLoopMutex);
        m_terminated = true;
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
    if (ini().throwPluginEventsFromDeviceAgent && !m_pluginEventThread)
    {
        NX_PRINT << __func__ << "(): Starting plugin event generation thread";
        m_pluginEventThread = std::make_unique<std::thread>([this]() { processPluginEvents(); });
    }
    else
    {
        NX_PRINT << __func__ << "()";
    }
}

bool DeviceAgent::pushCompressedVideoFrame(const ICompressedVideoPacket* videoFrame)
{
    if (engine()->needUncompressedVideoFrames())
    {
        NX_PRINT << "ERROR: Received compressed video frame, contrary to manifest.";
        return false;
    }

    NX_OUTPUT << __func__ << "(): timestamp " << videoFrame->timestampUs() << " us";
    ++m_frameCounter;
    m_lastVideoFrameTimestampUsec = videoFrame->timestampUs();
    return true;
}

bool DeviceAgent::pushUncompressedVideoFrame(const IUncompressedVideoFrame* videoFrame)
{
    if (!engine()->needUncompressedVideoFrames())
    {
        NX_PRINT << "ERROR: Received uncompressed video frame, contrary to manifest.";
        return false;
    }

    NX_OUTPUT << __func__ << "(): timestamp " << videoFrame->timestampUs() << " us";

    ++m_frameCounter;
    m_lastVideoFrameTimestampUsec = videoFrame->timestampUs();

    return checkFrame(videoFrame);
}

bool DeviceAgent::pullMetadataPackets(std::vector<IMetadataPacket*>* metadataPackets)
{
    NX_OUTPUT << __func__ << "() BEGIN";

    const char* logMessage = "";
    if (ini().generateObjects)
    {
        IMetadataPacket* const metadataPacket = cookSomeObjects();
        if (metadataPacket)
        {
            metadataPackets->push_back(metadataPacket);
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

    m_lastVideoFrameTimestampUsec = 0;

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

    if (ini().generateEvents)
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
    while (!m_terminated)
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
            if (m_terminated)
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

IMetadataPacket* DeviceAgent::cookSomeEvents()
{
    std::string caption;
    std::string description;
    bool isActive;
    
    if (m_eventTypeId == kLineCrossingEventType)
    {
        m_eventTypeId = kObjectInTheAreaEventType;
        caption = "Object in the Area (caption)";
        description = "Object in the Area (description)";
        isActive = 1;
    }
    else
    {
        m_eventTypeId = kLineCrossingEventType;
        caption = "Line Crossing (caption)";
        description = "Line Crossing (description)";
        isActive = 0;
    }

    auto eventMetadata = makePtr<EventMetadata>();
    eventMetadata->setCaption(caption);
    eventMetadata->setDescription(description);
    eventMetadata->setAuxiliaryData(R"json({ "auxiliaryData": "someJson" })json");
    eventMetadata->setTypeId(m_eventTypeId);
    eventMetadata->setIsActive(isActive);

    auto eventMetadataPacket = new EventMetadataPacket();
    eventMetadataPacket->setTimestampUs(usSinceEpoch());
    eventMetadataPacket->setDurationUs(0);
    eventMetadataPacket->addItem(eventMetadata.get());

    NX_OUTPUT << "Firing event: "
        << "type: " << m_eventTypeId
        << ", isActive: " << ((isActive) ? "true" : "false");

    return eventMetadataPacket;
}

IMetadataPacket* DeviceAgent::cookSomeObjects()
{
    if (m_lastVideoFrameTimestampUsec == 0)
        return nullptr;

    if (m_frameCounter % ini().generateObjectsEveryNFrames != 0)
        return nullptr;

    auto objectMetadata = makePtr<ObjectMetadata>();

    double dt = m_objectCounter / 32.0;
    ++m_objectCounter;
    double intPart;
    dt = modf(dt, &intPart) * 0.75;
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
        m_objectId = UuidHelper::randomUuid();
        m_currentObjectIndex = sequentialNumber;
        m_objectTypeId = kObjectTypes.at(m_currentObjectTypeIndex);
	++m_currentObjectTypeIndex;
	if (m_currentObjectTypeIndex == (int) kObjectTypes.size())
	    m_currentObjectTypeIndex = 0;
    }

    objectMetadata->setAuxiliaryData(R"json({ "auxiliaryData": "someJson2" })json");
    objectMetadata->setTypeId(m_objectTypeId);
    objectMetadata->setId(m_objectId);
    objectMetadata->setBoundingBox(IObjectMetadata::Rect((float) dt, (float) dt, 0.25F, 0.25F));

    if (dt < 0.5)
    {
        m_previewAttributesGenerated = false;
    }
    else if (dt > 0.5 && !m_previewAttributesGenerated && ini().generatePreviewAttributes)
    {
        m_previewAttributesGenerated = true;

        // Make a box smaller than the one in setBoundingBox() to make the change visible.
        objectMetadata->setAttributes({
            {IAttribute::Type::number, "nx.sys.preview.timestampUs",
                std::to_string(m_lastVideoFrameTimestampUsec)},
            {IAttribute::Type::number, "nx.sys.preview.boundingBox.x", std::to_string(dt)},
            {IAttribute::Type::number, "nx.sys.preview.boundingBox.y", std::to_string(dt)},
            {IAttribute::Type::number, "nx.sys.preview.boundingBox.width", "0.1"},
            {IAttribute::Type::number, "nx.sys.preview.boundingBox.height", "0.1"}
        });
    }

    auto objectMetadataPacket = new ObjectMetadataPacket();

    objectMetadataPacket->setTimestampUs(m_lastVideoFrameTimestampUsec);
    objectMetadataPacket->setDurationUs(0);
    objectMetadataPacket->addItem(objectMetadata.get());
    return objectMetadataPacket;
}

int64_t DeviceAgent::usSinceEpoch() const
{
    using namespace std::chrono;
    return duration_cast<microseconds>(
        system_clock::now().time_since_epoch()).count();
}

bool DeviceAgent::checkFrame(const IUncompressedVideoFrame* frame) const
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

    for (int plane = 0; plane < frame->planeCount(); ++plane)
    {
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
        }

        // Hex-dump some bytes from raw pixel data.
        if (NX_DEBUG_ENABLE_OUTPUT)
        {
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
    }

    return true;
}

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
