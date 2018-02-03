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
    static const std::string kLineCrossingEventGuidStr = nxpt::NxGuidHelper::toStdString(
        kLineCrossingEventGuid);
    static const std::string kCarObjectGuidStr = nxpt::NxGuidHelper::toStdString(
        kCarObjectGuid);

    return R"json(
        {
            "supportedEventTypes": [
                ")json" + kLineCrossingEventGuidStr + R"json("
                "{B0E64044-FFA3-4B7F-807A-060C1FE5A04C}"
            ],
            "supportedObjectTypes": [
                ")json" + kCarObjectGuidStr + R"json("
            ]
            "objectActions": [
                {
                    "id": "nx.stub.addToList",
                    "name": {
                        "value": "Add to list"
                    },
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
                    ],
                },
                {
                    "id": "nx.stub.addPerson",
                    "name": {
                        "value": "Add person (URL-based)"
                    },
                    "supportedObjectTypes": [
                        ")json" + kCarObjectGuidStr + R"json("
                    ]
                }
            ]
        }
    )json";
}

bool CameraManager::pushVideoFrame(const CommonCompressedVideoPacket* videoFrame)
{
    NX_OUTPUT << __func__ << "() BEGIN";
    m_videoFrame = videoFrame;
    NX_OUTPUT << __func__ << "() END -> true";
    return true;
}

bool CameraManager::pullMetadataPackets(std::vector<MetadataPacket*>* metadataPackets)
{
    NX_OUTPUT << __func__ << "() BEGIN";
    metadataPackets->push_back(cookSomeObjects());
    m_videoFrame = nullptr;
    NX_OUTPUT << __func__ << "() END -> true";
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

void CameraManager::executeAction(
    const std::string& actionId,
    const nx::sdk::metadata::Object* object,
    const std::map<std::string, std::string>& params,
    std::string* outActionUrl,
    std::string* outMessageToUser,
    Error* error)
{
    if (actionId == "nx.stub.addToList")
    {
        NX_PRINT << __func__ << "(): nx.stub.addToList; returning a message with param values.";
        *outMessageToUser = std::string("Your param values are: ")
            + "paramA: [" + params.at("paramA") + "], "
            + "paramB: [" + params.at("paramB") + "]";

    }
    else if (actionId == "nx.stub.addPerson")
    {
        *outActionUrl = "http://internal.server/addPerson?objectId=" +
            nxpt::NxGuidHelper::toStdString(object->id());
        NX_PRINT << __func__ << "(): nx.stub.addPerson; returning URL: [" << *outActionUrl << "]";
    }
    else
    {
        NX_PRINT << __func__ << "(): ERROR: Unsupported action: [" << actionId << "]";
        *error = Error::unknownError;
    }
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
    if (!m_videoFrame)
        return nullptr;

    auto commonObject = new CommonObject();
    nxpl::NX_GUID objectId = //< To be binary modified to be unique for each object.
        {{0xB5,0x29,0x4F,0x25,0x4F,0xE6,0x46,0x47,0xB8,0xD1,0xA0,0x72,0x9F,0x70,0xF2,0xD1}};

    commonObject->setAuxilaryData(R"json({ "auxilaryData": "someJson2" })json");
    commonObject->setTypeId(kCarObjectGuid);

    double dt = m_counterObjects++ / 32.0;
    double intPart;
    dt = modf(dt, &intPart) * 0.75;

    *reinterpret_cast<int*>(objectId.bytes) = static_cast<int>(intPart);
    commonObject->setId(objectId);

    commonObject->setBoundingBox(Rect(dt, dt, 0.25, 0.25));

    auto objectPacket = new CommonObjectsMetadataPacket();
    objectPacket->setTimestampUsec(m_videoFrame->timestampUsec());
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

} // namespace stub
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
