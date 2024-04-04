// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <nx/kit/utils.h>
#include <nx/sdk/helpers/uuid_helper.h>

#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_track_best_shot_packet.h>
#include <nx/sdk/analytics/helpers/object_track_title_packet.h>

#include <nx/vms_server_plugins/analytics/stub/utils.h>

#include "settings.h"
#include "stub_analytics_plugin_best_shots_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace best_shots {

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using Uuid = nx::sdk::Uuid;

static const int kTrackLength = 200;
static const float kMaxObjectHeight = 0.25F;
static const float kObjectWidth = 0.15F;
static const float kDistanceBetweenObjects = 0.05F;

static const std::string kObjectTypeId = "nx.stub.objectBestShotDemo";

/*static*/ DeviceAgent::BestShotGenerationPolicy DeviceAgent::bestShotGenerationPolicyFromString(
    const std::string& str)
{
    if (str == kUrlBestShotGenerationPolicy)
        return BestShotGenerationPolicy::url;

    if (str == kImageBestShotGenerationPolicy)
        return BestShotGenerationPolicy::image;

    return BestShotGenerationPolicy::fixed;
}

/*static*/ DeviceAgent::TitleGenerationPolicy DeviceAgent::titleGenerationPolicyFromString(
    const std::string& str)
{
    if (str == kUrlTitleGenerationPolicy)
        return TitleGenerationPolicy::url;

    if (str == kImageTitleGenerationPolicy)
        return TitleGenerationPolicy::image;

    return TitleGenerationPolicy::url;
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
    return /*suppress newline*/ 1 + (const char*)
R"json(
{
    "supportedTypes":
    [
        { "objectTypeId": ")json" + kObjectTypeId + R"json(" }
    ],
    "typeLibrary":
    {
        "objectTypes":
        [
            {
                "id": ")json" + kObjectTypeId + R"json(",
                "name": "Stub: Object Best Shot Demo"
            }
        ]
    }

}
)json";
}

bool DeviceAgent::pushCompressedVideoFrame(const ICompressedVideoPacket* videoPacket)
{
    m_lastFrameTimestampUs = videoPacket->timestampUs();

    Ptr<IObjectMetadataPacket> objectMetadataPacket = generateObjects();
    pushMetadataPacket(objectMetadataPacket.releasePtr());

    std::vector<Ptr<IObjectTrackBestShotPacket>> bestShotPackets = generateBestShots();
    for (Ptr<IObjectTrackBestShotPacket>& bestShotPacket: bestShotPackets)
        pushMetadataPacket(bestShotPacket.releasePtr());

    std::vector<Ptr<IObjectTrackTitlePacket>> titlePackets = generateTitles();
    for (Ptr<IObjectTrackTitlePacket>& titlePacket: titlePackets)
        pushMetadataPacket(titlePacket.releasePtr());

    return true;
}

void DeviceAgent::doSetNeededMetadataTypes(
    nx::sdk::Result<void>* /*outValue*/,
    const nx::sdk::analytics::IMetadataTypes* /*neededMetadataTypes*/)
{
}

nx::sdk::Result<const nx::sdk::ISettingsResponse*> DeviceAgent::settingsReceived()
{
    std::map<std::string, std::string> settings = currentSettings();

    m_bestShotGenerationCounterByTrackId.clear();

    m_bestShotGenerationContext.policy = bestShotGenerationPolicyFromString(
        settings[kBestShotGenerationPolicySetting]);

    const std::string bestShotImagePath = settings[kBestShotImagePathSetting];
    if (!bestShotImagePath.empty())
    {
        m_bestShotGenerationContext.imageData = loadFile(bestShotImagePath);
        m_bestShotGenerationContext.imageDataFormat = imageFormatFromPath(bestShotImagePath);
    }

    m_bestShotGenerationContext.url = settings[kBestShotUrlSetting];

    nx::kit::utils::fromString(
        settings[kTopLeftXSetting],
        &m_bestShotGenerationContext.fixedBestShotBoundingBox.x);

    nx::kit::utils::fromString(
        settings[kTopLeftYSetting],
        &m_bestShotGenerationContext.fixedBestShotBoundingBox.y);

    nx::kit::utils::fromString(
        settings[kWidthSetting],
        &m_bestShotGenerationContext.fixedBestShotBoundingBox.width);

    nx::kit::utils::fromString(
        settings[kHeightSetting],
        &m_bestShotGenerationContext.fixedBestShotBoundingBox.height);

    nx::kit::utils::fromString(
        settings[kFrameNumberToGenerateBestShotSetting],
        &m_bestShotGenerationContext.frameNumberToGenerateBestShot);

    int objectCount = 0;
    nx::kit::utils::fromString(settings[kObjectCountSetting], &objectCount);
    m_trackContexts.clear();

    for (int i = 0; i < objectCount; ++i)
    {
        TrackContext trackContext;
        trackContext.trackId = UuidHelper::randomUuid();

        m_bestShotGenerationCounterByTrackId[trackContext.trackId] =
            m_bestShotGenerationContext.frameNumberToGenerateBestShot;

        trackContext.boundingBox.height = std::min(
            kMaxObjectHeight,
            (1.0F - kDistanceBetweenObjects * objectCount) / objectCount);
        trackContext.boundingBox.width = kObjectWidth;
        trackContext.boundingBox.x = 0.0F;
        trackContext.boundingBox.y = (i + 1) * kDistanceBetweenObjects
            + i * trackContext.boundingBox.height;

        m_trackContexts.push_back(std::move(trackContext));
    }

    m_titleGenerationContext.policy = titleGenerationPolicyFromString(
        settings[kTitleGenerationPolicySetting]);

    const std::string titleImagePath = settings[kTitleImagePathSetting];
    if (!titleImagePath.empty())
    {
        m_titleGenerationContext.imageData = loadFile(titleImagePath);
        m_titleGenerationContext.imageDataFormat = imageFormatFromPath(titleImagePath);
    }

    m_titleGenerationContext.url = settings[kTitleUrlSetting];

    m_titleGenerationContext.text = settings[kTitleTextSetting];

    if (m_titleGenerationContext.text.empty())
        m_titleGenerationContext.text = "Default Title";

    return nullptr;
}

Ptr<IObjectMetadataPacket> DeviceAgent::generateObjects()
{
    auto objectMetadataPacket = makePtr<ObjectMetadataPacket>();
    objectMetadataPacket->setTimestampUs(m_lastFrameTimestampUs);

    for (TrackContext& trackContext: m_trackContexts)
    {
        trackContext.boundingBox.x += 1.0F / kTrackLength;
        if (trackContext.boundingBox.x + trackContext.boundingBox.width > 1.0F)
        {
            trackContext.trackId = UuidHelper::randomUuid();
            trackContext.boundingBox.x = 0.0F;
            m_bestShotGenerationCounterByTrackId[trackContext.trackId] =
                m_bestShotGenerationContext.frameNumberToGenerateBestShot;
        }

        auto objectMetadata = makePtr<ObjectMetadata>();
        objectMetadata->setTypeId(kObjectTypeId);
        objectMetadata->setTrackId(trackContext.trackId);
        objectMetadata->setBoundingBox(trackContext.boundingBox);

        objectMetadataPacket->addItem(objectMetadata.get());
    }

    return objectMetadataPacket;
}

DeviceAgent::BestShotList DeviceAgent::generateBestShots()
{
    BestShotList result;

    auto it = m_bestShotGenerationCounterByTrackId.begin();
    while (it != m_bestShotGenerationCounterByTrackId.end())
    {
        const Uuid& trackId = it->first;
        int& counter = it->second;

        if (counter == 0)
        {
            if (m_bestShotGenerationContext.policy == BestShotGenerationPolicy::fixed)
                result.push_back(generateFixedBestShot(trackId));
            else if (m_bestShotGenerationContext.policy == BestShotGenerationPolicy::url)
                result.push_back(generateUrlBestShot(trackId));
            else if (m_bestShotGenerationContext.policy == BestShotGenerationPolicy::image)
                result.push_back(generateImageBestShot(trackId));

            it = m_bestShotGenerationCounterByTrackId.erase(it);
        }
        else
        {
            --counter;
            ++it;
        }
    }

    return result;
}

DeviceAgent::TitleList DeviceAgent::generateTitles()
{
    TitleList result;

    for (const TrackContext& trackContext: m_trackContexts)
    {
        auto titlePacket = makePtr<ObjectTrackTitlePacket>(trackContext.trackId,
            m_lastFrameTimestampUs);

        titlePacket->setText(m_titleGenerationContext.text);

        if (m_titleGenerationContext.policy == TitleGenerationPolicy::url)
        {
            titlePacket->setImageUrl(m_titleGenerationContext.url);
        }
        else if (m_titleGenerationContext.policy == TitleGenerationPolicy::image)
        {
            titlePacket->setImageDataFormat(m_titleGenerationContext.imageDataFormat);
            titlePacket->setImageData(m_titleGenerationContext.imageData);
        }

        result.push_back(titlePacket);
    }

    return result;
}

Ptr<IObjectTrackBestShotPacket> DeviceAgent::generateFixedBestShot(Uuid trackId)
{
    return makePtr<ObjectTrackBestShotPacket>(
        trackId,
        m_lastFrameTimestampUs,
        m_bestShotGenerationContext.fixedBestShotBoundingBox);
}

Ptr<IObjectTrackBestShotPacket> DeviceAgent::generateUrlBestShot(Uuid trackId)
{
    auto bestShotPacket = makePtr<ObjectTrackBestShotPacket>(
        trackId,
        m_lastFrameTimestampUs);

    bestShotPacket->setImageUrl(m_bestShotGenerationContext.url);

    return bestShotPacket;
}

Ptr<IObjectTrackBestShotPacket> DeviceAgent::generateImageBestShot(Uuid trackId)
{
    auto bestShotPacket = makePtr<ObjectTrackBestShotPacket>(
        trackId,
        m_lastFrameTimestampUs);

    bestShotPacket->setImageDataFormat(m_bestShotGenerationContext.imageDataFormat);
    bestShotPacket->setImageData(m_bestShotGenerationContext.imageData);

    return bestShotPacket;
}

} // namespace best_shots
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
