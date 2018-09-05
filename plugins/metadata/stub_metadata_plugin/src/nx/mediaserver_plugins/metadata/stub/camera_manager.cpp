#include "camera_manager.h"

#include <iostream>
#include <chrono>
#include <math.h>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/common_metadata_packet.h>
#include <nx/sdk/metadata/common_event.h>
#include <nx/sdk/metadata/common_object.h>

#define NX_PRINT_PREFIX (this->utils.printPrefix)
#include <nx/kit/debug.h>

#include "stub_metadata_plugin_ini.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace stub {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

CameraManager::CameraManager(Plugin* plugin):
    CommonVideoFrameProcessingCameraManager(plugin, NX_DEBUG_ENABLE_OUTPUT)
{
}

std::string CameraManager::capabilitiesManifest()
{
    return R"json(
        {
            "supportedEventTypes": [
                ")json" + kLineCrossingEventType + R"json(",
                ")json" + kObjectInTheAreaEventType + R"json("
            ],
            "supportedObjectTypes": [
                ")json" + kCarObjectType + R"json("
            ],
            "settings": {
                "params": [
                    {
                        "id": "paramA",
                        "dataType": "Number",
                        "name": "Param A",
                        "description": "Number A"
                    },
                    {
                        "id": "paramB",
                        "dataType": "Enumeration",
                        "range": "b1,b3",
                        "name": "Param B",
                        "description": "Enumeration B"
                    }
                ]
            }
        }
    )json";
}

void CameraManager::settingsChanged()
{
    NX_PRINT << __func__ << "()";
}

bool CameraManager::pushCompressedVideoFrame(const CompressedVideoPacket* videoFrame)
{
    if (plugin()->needUncompressedVideoFrames())
    {
        NX_PRINT << "ERROR: Received compressed video frame, contrary to manifest.";
        return false;
    }

    NX_OUTPUT << __func__ << "(): timestamp " << videoFrame->timestampUsec() << " us";
    ++m_frameCounter;
    m_lastVideoFrameTimestampUsec = videoFrame->timestampUsec();
    return true;
}

bool CameraManager::pushUncompressedVideoFrame(const UncompressedVideoFrame* videoFrame)
{
    if (!plugin()->needUncompressedVideoFrames())
    {
        NX_PRINT << "ERROR: Received uncompressed video frame, contrary to manifest.";
        return false;
    }

    NX_OUTPUT << __func__ << "(): timestamp " << videoFrame->timestampUsec() << " us";

    ++m_frameCounter;
    m_lastVideoFrameTimestampUsec = videoFrame->timestampUsec();

    return checkFrame(videoFrame);
}

bool CameraManager::pullMetadataPackets(std::vector<MetadataPacket*>* metadataPackets)
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

Error CameraManager::startFetchingMetadata(const char* const* /*typeList*/, int /*typeListSize*/)
{
    NX_OUTPUT << __func__ << "() BEGIN";

    m_eventTypeId = kLineCrossingEventType; //< First event to produce.

    auto metadataDigger =
        [this]()
        {
            while (!m_stopping)
            {
                using namespace std::chrono_literals;
                pushMetadataPacket(cookSomeEvents());
                std::unique_lock<std::mutex> lock(m_eventGenerationLoopMutex);
                // Sleep until the next event needs to be generated, or the thread is ordered to
                // terminate (hence condition variable instead of sleep()). Return value (whether
                // the timeout has occurred) and spurious wake-ups are ignored.
                m_eventGenerationLoopCondition.wait_for(lock, 3000ms);
            }
        };

    if (ini().generateEvents)
    {
        NX_PRINT << "Starting event generation thread";
        m_thread.reset(new std::thread(metadataDigger));
    }

    NX_OUTPUT << __func__ << "() END -> noError";
    return Error::noError;
}

Error CameraManager::stopFetchingMetadata()
{
    NX_OUTPUT << __func__ << "() BEGIN";
    m_stopping = true;

    // Wake up event generation thread to avoid waiting until its sleeping period expires.
    m_eventGenerationLoopCondition.notify_all();

    if (m_thread)
    {
        m_thread->join();
        m_thread.reset();
    }
    m_stopping = false;

    NX_OUTPUT << __func__ << "() END -> noError";
    return Error::noError;
}

//-------------------------------------------------------------------------------------------------
// private

MetadataPacket* CameraManager::cookSomeEvents()
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

MetadataPacket* CameraManager::cookSomeObjects()
{
    if (m_lastVideoFrameTimestampUsec == 0)
        return nullptr;

    if (m_frameCounter % ini().generateObjectsEveryNFrames != 0)
        return nullptr;

    auto commonObject = new CommonObject();

    commonObject->setAuxilaryData(R"json({ "auxilaryData": "someJson2" })json");
    commonObject->setTypeId(kCarObjectType);

    // To be binary modified to be unique for each object.
    nxpl::NX_GUID objectId =
        {{0xB5,0x29,0x4F,0x25,0x4F,0xE6,0x46,0x47,0xB8,0xD1,0xA0,0x72,0x9F,0x70,0xF2,0xD1}};

    double dt = m_objectCounter / 32.0;
    ++m_objectCounter;
    double intPart;
    dt = modf(dt, &intPart) * 0.75;
    *reinterpret_cast<int*>(objectId.bytes) = static_cast<int>(intPart);
    commonObject->setId(objectId);

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
    objectPacket->setDurationUsec(1000000LL * 10);
    objectPacket->addItem(commonObject);
    return objectPacket;
}

int64_t CameraManager::usSinceEpoch() const
{
    using namespace std::chrono;
    return duration_cast<microseconds>(
        system_clock::now().time_since_epoch()).count();
}

bool CameraManager::checkFrame(const UncompressedVideoFrame* frame) const
{
    if (frame->pixelFormat() != plugin()->pixelFormat())
    {
        NX_PRINT << __func__ << "() ERROR: Video frame has pixel format "
            << pixelFormatToStdString(frame->pixelFormat()) << " instead of "
            << pixelFormatToStdString(plugin()->pixelFormat());
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
        const int bytesPerPlane =
            (frame->height() / pixelFormatDescriptor->chromaHeightFactor) * frame->lineSize(plane)
            * (plane == 0 ? 1 : (pixelFormatDescriptor->lumaBitsPerPixel / 8));
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
                NX_PRINT_HEX_DUMP(nx::kit::debug::format(
                    "Plane %d bytes %d..%d of %d",
                    plane, dumpOffset, dumpOffset + dumpSize - 1, frame->dataSize(plane)).c_str(),
                    frame->data(plane) + dumpOffset, dumpSize);
            }
        }
    }

    return true;
}

} // namespace stub
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
