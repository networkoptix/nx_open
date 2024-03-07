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

/**
 * @param deviceInfo Various information about the related device, such as its id, vendor, model,
 *     etc.
 */
DeviceAgent::DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo):
    // Call the DeviceAgent helper class constructor telling it to verbosely report to stderr.
    ConsumingDeviceAgent(deviceInfo, /*enableOutput*/ true)
{
}

DeviceAgent::~DeviceAgent()
{
}

/**
 *  @return JSON with the particular structure. Note that it is possible to fill in the values
 * that are not known at compile time, but should not depend on the DeviceAgent settings.
 */
std::string DeviceAgent::manifestString() const
{
    // Tell the Server that the plugin can generate the events and objects of certain types.
    // Id values are strings and should be unique. Format of ids:
    // `{vendor_id}.{plugin_id}.{event_type_id/object_type_id}`.
    //
    // See the plugin manifest for the explanation of vendor_id and plugin_id.
    return /*suppress newline*/ 1 + (const char*) R"json(
{
    "supportedTypes":
    [
         { "objectTypeId": ")json" + kHelloWorldObjectType + R"json(" },
         { "eventTypeId": ")json" + kNewTrackEventType + R"json(" }
    ],
    "typeLibrary":
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
}
)json";
}

/**
 * Called when the Server sends a new compressed frame from a camera.
 */
bool DeviceAgent::pushCompressedVideoFrame(const ICompressedVideoPacket* videoFrame)
{
    ++m_frameIndex;
    m_lastVideoFrameTimestampUs = videoFrame->timestampUs();

    // Generate an Event on every kTrackFrameCount'th frame.
    if (m_frameIndex % kTrackFrameCount == 0)
    {
        auto eventMetadataPacket = generateEventMetadataPacket();
        // Send generated metadata packet to the Server.
        pushMetadataPacket(eventMetadataPacket.releasePtr());
    }

    return true; //< There were no errors while processing the video frame.
}

/**
 * Serves the similar purpose as pushMetadataPacket(). The differences are:
 * - pushMetadataPacket() is called by the plugin, while pullMetadataPackets() is called by Server.
 * - pushMetadataPacket() expects one metadata packet, while pullMetadataPacket expects the
 *     std::vector of them.
 *
 * There are no strict rules for deciding which method is "better". A rule of thumb is to use
 * pushMetadataPacket() when you generate one metadata packet and do not want to store it in the
 * class field, and use pullMetadataPackets otherwise.
 */
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

} // namespace sample
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
