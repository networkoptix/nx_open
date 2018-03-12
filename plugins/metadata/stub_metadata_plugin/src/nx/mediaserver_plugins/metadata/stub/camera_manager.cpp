#include "camera_manager.h"

#include <iostream>
#include <chrono>
#include <math.h>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/common_metadata_packet.h>
#include <nx/sdk/metadata/common_event.h>
#include <nx/sdk/metadata/common_object.h>
#include <nx/sdk/metadata/common_compressed_video_packet.h>

#define NX_DEBUG_ENABLE_OUTPUT true //< Stub plugin is itself a debug feature, thus is verbose.
#define NX_PRINT_PREFIX (std::string("[") + this->plugin()->name() + " CameraManager] ")
#include <nx/kit/debug.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace stub {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

CameraManager::CameraManager(Plugin* plugin): CommonVideoFrameProcessingCameraManager(plugin)
{
    NX_PRINT << "Created " << this;
    setEnableOutput(NX_DEBUG_ENABLE_OUTPUT); //< Base class is verbose when this descendant is.
}

CameraManager::~CameraManager()
{
    NX_PRINT << "Destroyed " << this;
}

std::string CameraManager::capabilitiesManifest()
{
    return R"json(
        {
            "supportedEventTypes": [
                ")json" + nxpt::NxGuidHelper::toStdString(kLineCrossingEventGuid) + R"json(",
                ")json" + nxpt::NxGuidHelper::toStdString(kObjectInTheAreaEventGuid) + R"json("
            ],
            "supportedObjectTypes": [
                ")json" + nxpt::NxGuidHelper::toStdString(kCarObjectGuid) + R"json("
            ],
            "settings": {
                "params": [
                    {
                        "id": "paramAId",
                        "dataType": "Number",
                        "name": "Param A",
                        "description": "Number A"
                    },
                    {
                        "id": "paramBId",
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

bool CameraManager::pushCompressedVideoFrame(const CommonCompressedVideoPacket* videoFrame)
{
    NX_OUTPUT << __func__ << "() BEGIN: timestamp " << videoFrame->timestampUsec() << " us";
    m_lastVideoFrameTimestampUsec = videoFrame->timestampUsec();
    NX_OUTPUT << __func__ << "() END -> true";
    return true;
}

bool CameraManager::pushUncompressedVideoFrame(const CommonUncompressedVideoFrame* videoFrame)
{
    NX_OUTPUT << __func__ << "() BEGIN: timestamp " << videoFrame->timestampUsec() << " us";

    m_lastVideoFrameTimestampUsec = videoFrame->timestampUsec();

    if (videoFrame->pixelFormat() == UncompressedVideoFrame::PixelFormat::bgra)
        return checkRgbFrame(videoFrame);

    if (videoFrame->pixelFormat() == UncompressedVideoFrame::PixelFormat::yuv420)
        return checkYuv420pFrame(videoFrame);

    return false;
}

bool CameraManager::pullMetadataPackets(std::vector<MetadataPacket*>* metadataPackets)
{
    NX_OUTPUT << __func__ << "() BEGIN";

    MetadataPacket* const metadataPacket = cookSomeObjects();
    if (metadataPacket)
        metadataPackets->push_back(metadataPacket);

    m_lastVideoFrameTimestampUsec = 0;

    NX_OUTPUT << __func__ << "() END -> true: Generated " << (metadataPacket ? 1 : 0)
        << " metadata packet(s)";
    return true;
}

Error CameraManager::startFetchingMetadata(nxpl::NX_GUID* /*typeList*/, int /*typeListSize*/)
{
    NX_OUTPUT << __func__ << "() BEGIN";

    m_eventTypeId = kLineCrossingEventGuid; //< First event to produce.

    auto metadataDigger =
        [this]()
        {
            while (!m_stopping)
            {
                pushMetadataPacket(cookSomeEvents());
                std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            }
        };

    m_thread.reset(new std::thread(metadataDigger));

    NX_OUTPUT << __func__ << "() END -> noError";
    return Error::noError;
}

Error CameraManager::stopFetchingMetadata()
{
    NX_OUTPUT << __func__ << "() BEGIN";
    m_stopping = true;
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
        if (m_eventTypeId == kLineCrossingEventGuid)
            m_eventTypeId = kObjectInTheAreaEventGuid;
        else
            m_eventTypeId = kLineCrossingEventGuid;

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
    eventPacket->addItem(commonEvent);

    NX_PRINT << "Firing event: "
        << "type: " << (
            (m_eventTypeId == kLineCrossingEventGuid)
            ? "LineCrossing"
            : (m_eventTypeId == kObjectInTheAreaEventGuid)
                ? "ObjectInTheArea"
                : "Unknown"
        )
        << ", isActive: " << ((m_counter == 1) ? "true" : "false");

    return eventPacket;
}

MetadataPacket* CameraManager::cookSomeObjects()
{
    if (m_lastVideoFrameTimestampUsec == 0)
        return nullptr;

    auto commonObject = new CommonObject();

    commonObject->setAuxilaryData(R"json({ "auxilaryData": "someJson2" })json");
    commonObject->setTypeId(kCarObjectGuid);

    // To be binary modified to be unique for each object.
    nxpl::NX_GUID objectId = {{
        0xB5, 0x29, 0x4F, 0x25, 0x4F, 0xE6, 0x46, 0x47,
        0xB8, 0xD1, 0xA0, 0x72, 0x9F, 0x70, 0xF2, 0xD1}};

    double dt = m_counterObjects++ / 32.0;
    double intPart;
    dt = modf(dt, &intPart) * 0.75;
    *reinterpret_cast<int*>(objectId.bytes) = static_cast<int>(intPart);
    commonObject->setId(objectId);

    commonObject->setBoundingBox(Rect((float) dt, (float) dt, 0.25F, 0.25F));

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

bool CameraManager::checkYuv420pFrame(const sdk::metadata::CommonUncompressedVideoFrame* videoFrame) const
{
    if (videoFrame->planeCount() != 3)
    {
        NX_PRINT << __func__ << "() END -> false: videoFrame->planeCount() is "
            << videoFrame->planeCount() << " instead of 3";
        return false;
    }

    const auto expectedPixelFormat = UncompressedVideoFrame::PixelFormat::yuv420;
    if (videoFrame->pixelFormat() != expectedPixelFormat)
    {
        NX_PRINT << __func__ << "() END -> false: videoFrame->pixelFormat() is "
            << (int)videoFrame->pixelFormat() << " instead of " << (int)expectedPixelFormat;
        return false;
    }

    // Dump 8 bytes at offset 16 of the plane 0.
    if (videoFrame->dataSize(0) < 44)
    {
        NX_PRINT << __func__ << "(): videoFrame->dataSize(0) == " << videoFrame->dataSize(0)
            << ", which is suspiciously low";
    }
    else
    {
        // Hex dump some 8 bytes from raw pixel data.
        NX_PRINT_HEX_DUMP("bytes 36..43", videoFrame->data(0) + 36, 8);
    }

    NX_OUTPUT << __func__ << "() END -> true";
}

bool CameraManager::checkRgbFrame(const sdk::metadata::CommonUncompressedVideoFrame* videoFrame) const
{
    // TODO: #dmishin check BGRA frames somehow.
    NX_OUTPUT << __func__ << "() END -> true";
    return true;
}

} // namespace stub
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
