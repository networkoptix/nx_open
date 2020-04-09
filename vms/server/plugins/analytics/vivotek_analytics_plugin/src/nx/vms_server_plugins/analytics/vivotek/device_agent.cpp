#include "device_agent.h"

#include <chrono>

#include <nx/kit/json.h>
#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::kit;
using namespace nx::sdk;
using namespace nx::sdk::analytics;

DeviceAgent::DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo):
    ConsumingDeviceAgent(deviceInfo, /*enableOutput*/ true)
{
}

DeviceAgent::~DeviceAgent()
{
}

std::string DeviceAgent::manifestString() const
{
    return Json(Json::object{
        {"eventTypes", Json::array{
            Json::object{
                {"id", kNewTrackEventType},
                {"name", "New track started"},
            },
        }},
        {"objectTypes", Json::array{
            Json::object{
                {"id", kHelloWorldObjectType},
                {"name", "Hello, World!"},
            },
        }},
    }).dump();
}

bool DeviceAgent::pushUncompressedVideoFrame(const IUncompressedVideoFrame* videoFrame)
{
    ++m_frameIndex;
    m_lastVideoFrameTimestampUs = videoFrame->timestampUs();

    auto eventMetadataPacket = generateEventMetadataPacket();
    if (eventMetadataPacket)
    {
        // Send generated metadata packet to the Server.
        pushMetadataPacket(eventMetadataPacket.releasePtr());
    }

    return true; //< There were no errors while processing the video frame.
}

bool DeviceAgent::pullMetadataPackets(std::vector<IMetadataPacket*>* metadataPackets)
{
    metadataPackets->push_back(generateObjectMetadataPacket().releasePtr());

    return true; //< There were no errors while filling metadataPackets.
}

void DeviceAgent::doSetNeededMetadataTypes(
    nx::sdk::Result<void>* /*outValue*/,
    const nx::sdk::analytics::IMetadataTypes* /*neededMetadataTypes*/)
{
}

//-------------------------------------------------------------------------------------------------
// private

Ptr<IMetadataPacket> DeviceAgent::generateEventMetadataPacket()
{
    // Generate event every kTrackFrameCount'th frame.
    if (m_frameIndex % kTrackFrameCount != 0)
        return nullptr;

    // EventMetadataPacket contains arbitrary number of EventMetadata.
    const auto eventMetadataPacket = makePtr<EventMetadataPacket>();
    // Bind event metadata packet to the last video frame using a timestamp.
    eventMetadataPacket->setTimestampUs(m_lastVideoFrameTimestampUs);
    // Zero duration means that the event is not sustained, but momental.
    eventMetadataPacket->setDurationUs(0);

    // EventMetadata contains an information about event.
    const auto eventMetadata = makePtr<EventMetadata>();
    // Set all required fields.
    eventMetadata->setTypeId(kNewTrackEventType);
    eventMetadata->setIsActive(true);
    eventMetadata->setCaption("New sample plugin track started");
    eventMetadata->setDescription("New track #" + std::to_string(m_trackIndex) + " started");

    eventMetadataPacket->addItem(eventMetadata.get());

    // Generate index and track id for the next track.
    ++m_trackIndex;
    m_trackId = nx::sdk::UuidHelper::randomUuid();

    return eventMetadataPacket;
}

Ptr<IMetadataPacket> DeviceAgent::generateObjectMetadataPacket()
{
    // ObjectMetadataPacket contains arbitrary number of ObjectMetadata.
    const auto objectMetadataPacket = makePtr<ObjectMetadataPacket>();

    // Bind the object metadata to the last video frame using a timestamp.
    objectMetadataPacket->setTimestampUs(m_lastVideoFrameTimestampUs);
    objectMetadataPacket->setDurationUs(0);

    // ObjectMetadata contains information about an object on the frame.
    const auto objectMetadata = makePtr<ObjectMetadata>();
    // Set all required fields.
    objectMetadata->setTypeId(kHelloWorldObjectType);
    objectMetadata->setTrackId(m_trackId);

    // Calculate bounding box coordinates each frame so that it moves from the top left corner
    // to the bottom right corner during kTrackFrameCount frames.
    static constexpr float d = 0.5F / kTrackFrameCount;
    static constexpr float width = 0.5F;
    static constexpr float height = 0.5F;
    const int frameIndexInsideTrack = m_frameIndex % kTrackFrameCount;
    const float x = d * frameIndexInsideTrack;
    const float y = d * frameIndexInsideTrack;
    objectMetadata->setBoundingBox(Rect(x, y, width, height));

    objectMetadataPacket->addItem(objectMetadata.get());

    return objectMetadataPacket;
}

} // namespace nx::vms_server_plugins::analytics::vivotek
