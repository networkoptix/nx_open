#include "device_agent.h"

#include <iostream>
#include <chrono>
#include <ctime>

#include <plugins/plugin_tools.h>
#include <nx/sdk/analytics/common_metadata_packet.h>
#include <nx/sdk/analytics/common_event.h>
#include <nx/sdk/analytics/common_object.h>
#include <nx/sdk/common_settings.h>

#define NX_PRINT_PREFIX (this->utils.printPrefix)
#include <nx/kit/debug.h>

#include "stub_analytics_plugin_ini.h"

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace stub {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

DeviceAgent::DeviceAgent(Engine* engine):
    CommonVideoFrameProcessingDeviceAgent(engine, NX_DEBUG_ENABLE_OUTPUT),
    m_objectId{{0xB5,0x29,0x4F,0x25,0x4F,0xE6,0x46,0x47,0xB8,0xD1,0xA0,0x72,0x9F,0x70,0xF2,0xD1}}
{
    // TODO: #vkutin #mshevchenko Replace with true UUID generation when possible.
    *reinterpret_cast<void**>(m_objectId.bytes + sizeof(m_objectId) - sizeof(void*)) = this;
}

DeviceAgent::~DeviceAgent()
{
    m_terminated.store(true);
    m_pluginEventGenerationLoopCondition.notify_all();
    if (m_pluginEventThread)
        m_pluginEventThread->join();
}

/**
 * DeviceAgent manifest may declare eventTypes and objectTypes similarly to how an Engine declares
 * them - semantically the set from the Engine manifest is joined with the set from the DeviceAgent
 * manifest. Also this manifest may declare supportedEventTypeIds and supportedObjectTypeIds lists
 * which are treated as white-list filters for the respective set.
 */
std::string DeviceAgent::manifest() const
{
    return /*suppress newline*/1 + R"json(
{
    "supportedEventTypeIds": [
        ")json" + kLineCrossingEventType + R"json(",
        ")json" + kObjectInTheAreaEventType + R"json("
    ],
    "supportedObjectTypeIds": [
        ")json" + kCarObjectType + R"json("
    ],
    "eventTypes": [
        {
            "id": ")json" + kLineCrossingEventType + R"json(",
            "name": {
                "value": "Line crossing"
            }
        },
        {
            "id": ")json" + kObjectInTheAreaEventType + R"json(",
            "name": {
                "value": "Object in the area"
            },
            "flags": "stateDependent|regionDependent"
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

bool DeviceAgent::pushCompressedVideoFrame(const CompressedVideoPacket* videoFrame)
{
    if (engine()->needUncompressedVideoFrames())
    {
        NX_PRINT << "ERROR: Received compressed video frame, contrary to manifest.";
        return false;
    }

    NX_OUTPUT << __func__ << "(): timestamp " << videoFrame->timestampUsec() << " us";
    ++m_frameCounter;
    m_lastVideoFrameTimestampUsec = videoFrame->timestampUsec();
    return true;
}

bool DeviceAgent::pushUncompressedVideoFrame(const UncompressedVideoFrame* videoFrame)
{
    if (!engine()->needUncompressedVideoFrames())
    {
        NX_PRINT << "ERROR: Received uncompressed video frame, contrary to manifest.";
        return false;
    }

    NX_OUTPUT << __func__ << "(): timestamp " << videoFrame->timestampUsec() << " us";

    ++m_frameCounter;
    m_lastVideoFrameTimestampUsec = videoFrame->timestampUsec();

    return checkFrame(videoFrame);
}

bool DeviceAgent::pullMetadataPackets(std::vector<MetadataPacket*>* metadataPackets)
{
    NX_OUTPUT << __func__ << "() BEGIN";

    const char* logMessage = "";
    if (ini().generateObjects)
    {
        MetadataPacket* const metadataPacket = cookSomeObjects();
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
        static const std::chrono::seconds kPluginEventGenerationPeriod{10};
        std::unique_lock<std::mutex> lock(m_pluginEventGenerationLoopMutex);
        m_pluginEventGenerationLoopCondition.wait_for(lock, kPluginEventGenerationPeriod);
    }
}

nx::sdk::Settings* DeviceAgent::pluginSideSettings() const
{
    auto settings = new nx::sdk::CommonSettings();
    settings->addSetting("plugin_side_number", "100");

    return settings;
}

//-------------------------------------------------------------------------------------------------
// private

MetadataPacket* DeviceAgent::cookSomeEvents()
{
    ++m_counter;
    if (m_counter > 1)
    {
        if (m_eventTypeId == kLineCrossingEventType)
            m_eventTypeId = kObjectInTheAreaEventType;
        else
            m_eventTypeId = kLineCrossingEventType;

        m_counter = 0;
    }

    auto commonEvent = new CommonEvent();
    commonEvent->setCaption("Line crossing (caption)");
    commonEvent->setDescription("Line crossing (description)");
    commonEvent->setAuxilaryData(R"json({ "auxilaryData": "someJson" })json");
    commonEvent->setIsActive(m_counter == 1);
    commonEvent->setTypeId(m_eventTypeId);

    auto eventPacket = new CommonEventsMetadataPacket();
    eventPacket->setTimestampUsec(usSinceEpoch());
    eventPacket->setDurationUsec(0);
    eventPacket->addItem(commonEvent);

    NX_OUTPUT << "Firing event: "
        << "type: " << m_eventTypeId << ", isActive: " << ((m_counter == 1) ? "true" : "false");

    return eventPacket;
}

MetadataPacket* DeviceAgent::cookSomeObjects()
{
    if (m_lastVideoFrameTimestampUsec == 0)
        return nullptr;

    if (m_frameCounter % ini().generateObjectsEveryNFrames != 0)
        return nullptr;

    auto commonObject = new CommonObject();

    commonObject->setAuxilaryData(R"json({ "auxilaryData": "someJson2" })json");
    commonObject->setTypeId(kCarObjectType);

    double dt = m_objectCounter / 32.0;
    ++m_objectCounter;
    double intPart;
    dt = modf(dt, &intPart) * 0.75;
    const int sequentialNumber = static_cast<int>(intPart);

    if (m_currentObjectIndex != sequentialNumber)
    {
        // TODO: #vkutin #mshevchenko Replace with true UUID generation when possible.
        std::time(reinterpret_cast<time_t*>(m_objectId.bytes)); //< Make ID pseudo-unique.
        m_currentObjectIndex = sequentialNumber;
    }

    commonObject->setId(m_objectId);
    commonObject->setBoundingBox(Rect((float) dt, (float) dt, 0.25F, 0.25F));

    if (dt < 0.5)
    {
        m_previewAttributesGenerated = false;
    }
    else if (dt > 0.5 && !m_previewAttributesGenerated && ini().generatePreviewAttributes)
    {
        m_previewAttributesGenerated = true;

        // Make a box smaller than the one in setBoundingBox() to make the change visible.
        commonObject->setAttributes({
            {AttributeType::number, "nx.sys.preview.timestampUs",
                std::to_string(m_lastVideoFrameTimestampUsec)},
            {AttributeType::number, "nx.sys.preview.boundingBox.x", std::to_string(dt)},
            {AttributeType::number, "nx.sys.preview.boundingBox.y", std::to_string(dt)},
            {AttributeType::number, "nx.sys.preview.boundingBox.width", "0.1"},
            {AttributeType::number, "nx.sys.preview.boundingBox.height", "0.1"}
        });
    }

    auto objectPacket = new CommonObjectsMetadataPacket();

    objectPacket->setTimestampUsec(m_lastVideoFrameTimestampUsec);
    objectPacket->setDurationUsec(0);
    objectPacket->addItem(commonObject);
    return objectPacket;
}

int64_t DeviceAgent::usSinceEpoch() const
{
    using namespace std::chrono;
    return duration_cast<microseconds>(
        system_clock::now().time_since_epoch()).count();
}

bool DeviceAgent::checkFrame(const UncompressedVideoFrame* frame) const
{
    if (frame->pixelFormat() != engine()->pixelFormat())
    {
        NX_PRINT << __func__ << "() ERROR: Video frame has pixel format "
            << pixelFormatToStdString(frame->pixelFormat()) << " instead of "
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
        if (frame->handle() != 0)
        {
            if (frame->data(0) != nullptr)
            {
                NX_PRINT << __func__
                    << "() ERROR: data() is not null while handle() is not zero";
                return false;
            }
            if (!frame->map())
            {
                NX_PRINT << __func__ << "() ERROR: map() failed";
                return false;
            }
            NX_PRINT_HEX_DUMP("First byte of frame data(0)", frame->data(0), 1);
            frame->unmap();
        }
        else if (NX_DEBUG_ENABLE_OUTPUT)
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
                    nx::kit::debug::format("Plane %d bytes %d..%d of %d",
                        plane, dumpOffset, dumpOffset + dumpSize - 1, frame->dataSize(plane)).c_str(),
                    frame->data(plane) + dumpOffset, dumpSize);
            }
        }
    }

    return true;
}

} // namespace stub
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
