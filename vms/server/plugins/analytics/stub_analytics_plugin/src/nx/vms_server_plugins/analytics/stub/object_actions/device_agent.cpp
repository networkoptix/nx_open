// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include "common.h"

#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>

#include "stub_analytics_plugin_object_actions_ini.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_actions {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

static const int kTrackLength = 500;

DeviceAgent::DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo):
    ConsumingDeviceAgent(deviceInfo, ini().enableOutput)
{
}

DeviceAgent::~DeviceAgent()
{
}

std::string DeviceAgent::manifestString() const
{
    return /*suppress newline*/ 1 + (const char*)
R"json(
{
    "supportedTypes":
    [
        { "objectTypeId": ")json" + kObjectTypeId + R"json(" }
    ]
}
)json";
}

bool DeviceAgent::pushCompressedVideoFrame(const ICompressedVideoPacket* videoPacket)
{
    NX_OUTPUT << "Pushing compressed video packet, timestamp "
        << videoPacket->timestampUs() << " us; ";

    Ptr<IObjectMetadataPacket> objectMetadataPacket = generateObject(videoPacket->timestampUs());
    pushMetadataPacket(objectMetadataPacket.releasePtr());
    return true;
}

void DeviceAgent::doSetNeededMetadataTypes(
    nx::sdk::Result<void>* /*outValue*/,
    const nx::sdk::analytics::IMetadataTypes* /*neededMetadataTypes*/)
{
}

Ptr<IObjectMetadataPacket> DeviceAgent::generateObject(int64_t timestampUs)
{
    NX_OUTPUT << "Generating object for frame with timestamp " << timestampUs << " us; ";

    if (m_trackContext.trackId.isNull()
        || m_trackContext.boundingBox.x + m_trackContext.boundingBox.width > 1.0)
    {
        m_trackContext.trackId = UuidHelper::randomUuid();
        m_trackContext.boundingBox.x = 0;
        m_trackContext.boundingBox.y = 0.33F;
        m_trackContext.boundingBox.height = 0.33F;
        m_trackContext.boundingBox.width = 0.2F;
    }
    else
    {
        m_trackContext.boundingBox.x += 1.0F / kTrackLength;
    }

    auto objectMetadata = makePtr<ObjectMetadata>();
    objectMetadata->setTypeId(kObjectTypeId);
    objectMetadata->setBoundingBox(m_trackContext.boundingBox);
    objectMetadata->setTrackId(m_trackContext.trackId);

    auto objectMetadataPacket = makePtr<ObjectMetadataPacket>();
    objectMetadataPacket->setTimestampUs(timestampUs);
    objectMetadataPacket->addItem(objectMetadata.get());

    return objectMetadataPacket;
}

} // namespace object_actions
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
