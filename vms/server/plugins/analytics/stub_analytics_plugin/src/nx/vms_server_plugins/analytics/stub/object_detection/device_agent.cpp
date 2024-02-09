// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <chrono>

#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>

#include "device_agent_manifest.h"
#include "object_attributes.h"
#include "../utils.h"
#include "stub_analytics_plugin_object_detection_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_detection {

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using Uuid = nx::sdk::Uuid;

static constexpr int kTrackLength = 200;
static constexpr float kMaxBoundingBoxWidth = 0.5F;
static constexpr float kMaxBoundingBoxHeight = 0.5F;
static constexpr float kFreeSpace = 0.1F;
const std::string DeviceAgent::kTimeShiftSetting = "timestampShiftMs";
const std::string DeviceAgent::kSendAttributesSetting = "sendAttributes";
const std::string DeviceAgent::kObjectTypeGenerationSettingPrefix = "objectTypeIdToGenerate.";

static Rect generateBoundingBox(int frameIndex, int trackIndex, int trackCount)
{
    Rect boundingBox;
    boundingBox.width = std::min((1.0F - kFreeSpace) / trackCount, kMaxBoundingBoxWidth);
    boundingBox.height = std::min(boundingBox.width, kMaxBoundingBoxHeight);
    boundingBox.x = 1.0F / trackCount * trackIndex + kFreeSpace / (trackCount + 1);
    boundingBox.y = std::max(
        0.0F,
        1.0F - boundingBox.height - (1.0F / kTrackLength) * (frameIndex % kTrackLength));

    return boundingBox;
}

static std::vector<Ptr<ObjectMetadata>> generateObjects(
    const std::map<std::string, std::map<std::string, std::string>>& attributesByObjectType,
    const std::set<std::string>& objectTypeIdsToGenerate,
    bool doGenerateAttributes)
{
    std::vector<Ptr<ObjectMetadata>> result;

    for (const auto& entry: attributesByObjectType)
    {
        const std::string& objectTypeId = entry.first;
        if (objectTypeIdsToGenerate.find(objectTypeId) == objectTypeIdsToGenerate.cend())
            continue;

        auto objectMetadata = makePtr<ObjectMetadata>();
        objectMetadata->setTypeId(objectTypeId);

        if (doGenerateAttributes)
        {
            const std::map<std::string, std::string>& attributes = entry.second;
            for (const auto& attribute: attributes)
                objectMetadata->addAttribute(makePtr<Attribute>(attribute.first, attribute.second));
        }

        result.push_back(std::move(objectMetadata));
    }

    return result;
}

Ptr<IMetadataPacket> DeviceAgent::generateObjectMetadataPacket(int64_t frameTimestampUs)
{
    auto metadataPacket = makePtr<ObjectMetadataPacket>();
    metadataPacket->setTimestampUs(frameTimestampUs);

    std::vector<Ptr<ObjectMetadata>> objects;
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        objects = generateObjects(kObjectAttributes, m_objectTypeIdsToGenerate, m_sendAttributes);
    }

    for (int i = 0; i < (int) objects.size(); ++i)
    {
        objects[i]->setBoundingBox(generateBoundingBox(m_frameIndex, i, objects.size()));
        objects[i]->setTrackId(trackIdByTrackIndex(i));

        metadataPacket->addItem(objects[i].get());
    }

    return metadataPacket;
}

DeviceAgent::DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo):
    ConsumingDeviceAgent(deviceInfo, ini().enableOutput)
{
}

DeviceAgent::~DeviceAgent()
{
}

std::string DeviceAgent::manifestString() const
{
    return kDeviceAgentManifest;
}

bool DeviceAgent::pushCompressedVideoFrame(const ICompressedVideoPacket* videoFrame)
{
    ++m_frameIndex;
    if ((m_frameIndex % kTrackLength) == 0)
        m_trackIds.clear();

    Ptr<IMetadataPacket> objectMetadataPacket = generateObjectMetadataPacket(
        videoFrame->timestampUs() + m_timestampShiftMs * 1000);

    pushMetadataPacket(objectMetadataPacket.releasePtr());

    return true;
}

void DeviceAgent::doSetNeededMetadataTypes(
    nx::sdk::Result<void>* /*outValue*/,
    const nx::sdk::analytics::IMetadataTypes* /*neededMetadataTypes*/)
{
}

nx::sdk::Result<const nx::sdk::ISettingsResponse*> DeviceAgent::settingsReceived()
{
    const std::lock_guard<std::mutex> lock(m_mutex);

    m_objectTypeIdsToGenerate.clear();

    const std::map<std::string, std::string>& settings = currentSettings();
    for (const auto& entry: settings)
    {
        const std::string& key = entry.first;
        const std::string& value = entry.second;
        if (startsWith(key, kObjectTypeGenerationSettingPrefix) && toBool(value))
            m_objectTypeIdsToGenerate.insert(key.substr(kObjectTypeGenerationSettingPrefix.size()));
        else if (key == kSendAttributesSetting)
            m_sendAttributes = toBool(value);
        else if (key == kTimeShiftSetting)
            m_timestampShiftMs = std::stoi(value);
    }

    return nullptr;
}

Uuid DeviceAgent::trackIdByTrackIndex(int trackIndex)
{
    while (trackIndex >= m_trackIds.size())
        m_trackIds.push_back(UuidHelper::randomUuid());

    return m_trackIds[trackIndex];
}

} // namespace object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
