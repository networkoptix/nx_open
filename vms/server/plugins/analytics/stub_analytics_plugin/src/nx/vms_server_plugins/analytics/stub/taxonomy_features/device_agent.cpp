// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <chrono>

#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk//helpers/settings_response.h>

#include "device_agent_manifest.h"
#include "object_generators.h"
#include "../utils.h"
#include "stub_analytics_plugin_taxonomy_features_ini.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace taxonomy_features {

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using Uuid = nx::sdk::Uuid;

static constexpr int kTrackLength = 200;
static constexpr float kMaxBoundingBoxWidth = 0.5F;
static constexpr float kMaxBoundingBoxHeight = 0.5F;
static constexpr float kFreeSpace = 0.1F;

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

Ptr<IMetadataPacket> DeviceAgent::generateObjectMetadataPacket(int64_t frameTimestampUs)
{
    auto metadataPacket = makePtr<ObjectMetadataPacket>();
    metadataPacket->setTimestampUs(frameTimestampUs);

    std::vector<Ptr<ObjectMetadata>> objects;

    if (m_settings.generateInstanceOfBaseObjectType)
        objects.push_back(generateInstanceOfBaseObjectType());
    if (m_settings.generateInstanceOfDerivedObjectType)
        objects.push_back(generateInstanceOfDerivedObjectType());
    if (m_settings.generateInstanceOfDerivedObjectTypeWithOmittedAttributes)
        objects.push_back(generateInstanceOfDerivedObjectTypeWithOmittedAttributes());
    if (m_settings.generateInstanceOfHiddenDerivedObjectType)
        objects.push_back(generateInstanceOfHiddenDerivedObjectType());
    if (m_settings. generateInstanceOfHiddenDerivedObjectTypeWithOwnAttributes)
        objects.push_back(generateInstanceOfHiddenDerivedObjectTypeWithOwnAttributes());
    if (m_settings.generateInstanceOfDerivedObjectTypeWithUnsupportedBase)
        objects.push_back(generateInstanceOfDerivedObjectTypeWithUnsupportedBase());
    if (m_settings.generateInstanceOfObjectTypeWithNumericAttibutes)
        objects.push_back(generateInstanceOfObjectTypeWithNumericAttibutes());
    if (m_settings. generateInstanceOfObjectTypeWithBooleanAttibutes)
        objects.push_back(generateInstanceOfObjectTypeWithBooleanAttibutes());
    if (m_settings.generateInstanceOfObjectTypeWithIcon)
        objects.push_back(generateInstanceOfObjectTypeWithIcon());
    if (m_settings. generateInstanceOfObjectTypeInheritedFromBaseTypeLibraryType)
        objects.push_back(generateInstanceOfObjectTypeInheritedFromBaseTypeLibraryType());
    if (m_settings.generateInstanceOfObjectTypeUsingBaseTypeLibraryEnumType)
        objects.push_back(generateInstanceOfObjectTypeUsingBaseTypeLibraryEnumType());
    if (m_settings.generateInstanceOfObjectTypeUsingBaseTypeLibraryColorType)
        objects.push_back(generateInstanceOfObjectTypeUsingBaseTypeLibraryColorType());
    if (m_settings.generateInstanceOfObjectTypeUsingBaseTypeLibraryObjectType)
        objects.push_back(generateInstanceOfObjectTypeUsingBaseTypeLibraryObjectType());
    if (m_settings.generateInstanceOfOfBaseTypeLibraryObjectType)
        objects.push_back(generateInstanceOfOfBaseTypeLibraryObjectType());
    if (m_settings.generateInstanceOfObjectTypeDeclaredInEngineManifest)
        objects.push_back(generateInstanceOfObjectTypeDeclaredInEngineManifest());
    if (m_settings.generateInstanceOfLiveOnlyObjectType)
        objects.push_back(generateInstanceOfLiveOnlyObjectType());
    if (m_settings.generateInstanceOfNonIndexableObjectType)
        objects.push_back(generateInstanceOfNonIndexableObjectType());
    if (m_settings.generateInstanceOfExtendedObjectType)
        objects.push_back(generateInstanceOfExtendedObjectType());
    if (m_settings.generateInstanceOfObjectTypeWithAttributeList)
        objects.push_back(generateInstanceOfObjectTypeWithAttributeList());

    for (int i = 0; i < (int) objects.size(); ++i)
    {
        objects[i]->setBoundingBox(generateBoundingBox(m_frameIndex, i, (int) objects.size()));
        objects[i]->setTrackId(trackIdByTrackIndex(i));

        metadataPacket->addItem(objects[i].get());
    }

    return metadataPacket;
}

DeviceAgent::DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo):
    ConsumingDeviceAgent(deviceInfo, ini().enableOutput, engine->plugin()->instanceId()),
    m_engine(engine)
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
        videoFrame->timestampUs());

    pushMetadataPacket(objectMetadataPacket.releasePtr());

    return true;
}

void DeviceAgent::doSetNeededMetadataTypes(
    nx::sdk::Result<void>* /*outValue*/,
    const nx::sdk::analytics::IMetadataTypes* /*neededMetadataTypes*/)
{
}

Result<const nx::sdk::ISettingsResponse*> DeviceAgent::settingsReceived()
{
    m_settings.generateInstanceOfBaseObjectType =
        toBool(settingValue("generateInstanceOfBaseObjectType"));
    m_settings.generateInstanceOfDerivedObjectType =
        toBool(settingValue("generateInstanceOfDerivedObjectType"));
    m_settings.generateInstanceOfDerivedObjectTypeWithOmittedAttributes =
        toBool(settingValue("generateInstanceOfDerivedObjectTypeWithOmittedAttributes"));
    m_settings.generateInstanceOfHiddenDerivedObjectType =
        toBool(settingValue("generateInstanceOfHiddenDerivedObjectType"));
    m_settings.generateInstanceOfHiddenDerivedObjectTypeWithOwnAttributes =
        toBool(settingValue("generateInstanceOfHiddenDerivedObjectTypeWithOwnAttributes"));
    m_settings.generateInstanceOfDerivedObjectTypeWithUnsupportedBase =
        toBool(settingValue("generateInstanceOfDerivedObjectTypeWithUnsupportedBase"));
    m_settings.generateInstanceOfObjectTypeWithNumericAttibutes =
        toBool(settingValue("generateInstanceOfObjectTypeWithNumericAttibutes"));
    m_settings.generateInstanceOfObjectTypeWithBooleanAttibutes =
        toBool(settingValue("generateInstanceOfObjectTypeWithBooleanAttibutes"));
    m_settings.generateInstanceOfObjectTypeWithIcon =
        toBool(settingValue("generateInstanceOfObjectTypeWithIcon"));
    m_settings.generateInstanceOfObjectTypeInheritedFromBaseTypeLibraryType =
        toBool(settingValue("generateInstanceOfObjectTypeInheritedFromBaseTypeLibraryType"));
    m_settings.generateInstanceOfObjectTypeUsingBaseTypeLibraryEnumType =
        toBool(settingValue("generateInstanceOfObjectTypeUsingBaseTypeLibraryEnumType"));
    m_settings.generateInstanceOfObjectTypeUsingBaseTypeLibraryColorType =
        toBool(settingValue("generateInstanceOfObjectTypeUsingBaseTypeLibraryColorType"));
    m_settings.generateInstanceOfObjectTypeUsingBaseTypeLibraryObjectType =
        toBool(settingValue("generateInstanceOfObjectTypeUsingBaseTypeLibraryObjectType"));
    m_settings.generateInstanceOfOfBaseTypeLibraryObjectType =
        toBool(settingValue("generateInstanceOfOfBaseTypeLibraryObjectType"));
    m_settings.generateInstanceOfObjectTypeDeclaredInEngineManifest =
        toBool(settingValue("generateInstanceOfObjectTypeDeclaredInEngineManifest"));
    m_settings.generateInstanceOfLiveOnlyObjectType =
        toBool(settingValue("generateInstanceOfLiveOnlyObjectType"));
    m_settings.generateInstanceOfNonIndexableObjectType =
        toBool(settingValue("generateInstanceOfNonIndexableObjectType"));
    m_settings.generateInstanceOfExtendedObjectType =
        toBool(settingValue("generateInstanceOfExtendedObjectType"));
    m_settings.generateInstanceOfObjectTypeWithAttributeList =
        toBool(settingValue("generateInstanceOfObjectTypeWithAttributeList"));

    return nullptr;
}

Uuid DeviceAgent::trackIdByTrackIndex(int trackIndex)
{
    while (trackIndex >= (int) m_trackIds.size())
        m_trackIds.push_back(UuidHelper::randomUuid());

    return m_trackIds[trackIndex];
}

} // namespace taxonomy_features
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
