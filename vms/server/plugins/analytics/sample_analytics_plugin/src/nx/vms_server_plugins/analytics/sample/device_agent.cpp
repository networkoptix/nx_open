// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <chrono>

#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace sample {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

static int64_t usSinceEpoch()
{
    using namespace std::chrono;
    return duration_cast<microseconds>(
        system_clock::now().time_since_epoch()).count();
}

DeviceAgent::DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo):
    VideoFrameProcessingDeviceAgent(deviceInfo, /*enableOutput*/true)
{
}

DeviceAgent::~DeviceAgent()
{
}

std::string DeviceAgent::manifestString() const
{
    return /*suppress newline*/1 + R"json(
{
    "eventTypes": [
        {
            "id": ")json" + kNewTrackEventType + R"json(",
            "name": "New track started"
        }
    ],
    "objectTypes": [
        {
            "id": ")json" + kHelloWorldObjectType + R"json(",
            "name": "Hello, World!"
        }
    ]
}
)json";
}

bool DeviceAgent::pushUncompressedVideoFrame(const IUncompressedVideoFrame* videoFrame)
{
    ++m_frameIndex;
    m_lastVideoFrameTimestampUs = videoFrame->timestampUs();

    auto eventMetadataPacket = generateEventMetadataPacket();
    if (eventMetadataPacket)
        pushMetadataPacket(eventMetadataPacket.releasePtr());

    return true;
}

bool DeviceAgent::pullMetadataPackets(std::vector<IMetadataPacket*>* metadataPackets)
{
    metadataPackets->push_back(generateObjectMetadataPacket().releasePtr());

    return true;
}

//-------------------------------------------------------------------------------------------------
// private

Ptr<IMetadataPacket> DeviceAgent::generateEventMetadataPacket()
{
    // Generate event with period kTrackFrameCount.
    if (m_frameIndex % kTrackFrameCount != 0)
        return nullptr;

    const auto eventMetadataPacket = makePtr<EventMetadataPacket>();
    eventMetadataPacket->setTimestampUs(usSinceEpoch());
    eventMetadataPacket->setDurationUs(0);

    const auto eventMetadata = makePtr<EventMetadata>();
    eventMetadata->setTypeId(kNewTrackEventType);
    eventMetadata->setIsActive(true);
    eventMetadata->setCaption("New sample plugin track started");
    eventMetadata->setDescription("New track #" + std::to_string(m_trackIndex) + " started");

    eventMetadataPacket->addItem(eventMetadata.get());

    ++m_trackIndex;
    m_trackId = nx::sdk::UuidHelper::randomUuid();

    return eventMetadataPacket;
}

Ptr<IMetadataPacket> DeviceAgent::generateObjectMetadataPacket()
{
    const auto objectMetadataPacket = makePtr<ObjectMetadataPacket>();

    // Binding the object metadata to the particular video frame.
    objectMetadataPacket->setTimestampUs(m_lastVideoFrameTimestampUs);

    objectMetadataPacket->setDurationUs(0);

    const auto objectMetadata = makePtr<ObjectMetadata>();
    objectMetadata->setTypeId(kHelloWorldObjectType);
    objectMetadata->setTrackId(m_trackId);

    // Calculate bounding box coordinates each frame so that it moves from the top left corner
    // to the bottom right corner in kTrackFrameCount frames.
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

} // namespace sample
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
