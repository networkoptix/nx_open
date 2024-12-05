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

    return TitleGenerationPolicy::fixed;
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

void DeviceAgent::generateTitleObject()
{
    auto objectMetadataPacket = makePtr<ObjectMetadataPacket>();
    objectMetadataPacket->setTimestampUs(m_lastFrameTimestampUs);

    for (TrackContext& trackContext: m_titleTrackContexts)
    {
        trackContext.boundingBox.x += 1.0F / kTrackLength;
        if (trackContext.boundingBox.x + trackContext.boundingBox.width > 1.0F)
        {
            trackContext.trackId = UuidHelper::randomUuid();
            trackContext.boundingBox.x = 0.0F;
            m_titleGenerationCounterByTrackId[trackContext.trackId] =
                m_titleGenerationContext.frameNumberToGenerateTitle;
        }

        auto objectMetadata = makePtr<ObjectMetadata>();
        objectMetadata->setTypeId(kObjectTypeId);
        objectMetadata->setTrackId(trackContext.trackId);
        objectMetadata->setBoundingBox(trackContext.boundingBox);

        objectMetadataPacket->addItem(objectMetadata.get());
    }

    pushMetadataPacket(objectMetadataPacket.releasePtr());
}

void DeviceAgent::generateBestShotObject()
{
    auto objectMetadataPacket = makePtr<ObjectMetadataPacket>();
    objectMetadataPacket->setTimestampUs(m_lastFrameTimestampUs);

    for (TrackContext& trackContext: m_bestShotTrackContexts)
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

    pushMetadataPacket(objectMetadataPacket.releasePtr());
}

void DeviceAgent::maybeGenerateBestShotAndTitle()
{
    if (m_generateBestShot)
    {
        generateBestShotObject();

        std::vector<Ptr<IObjectTrackBestShotPacket>> bestShotPackets = generateBestShots();
        for (Ptr<IObjectTrackBestShotPacket>& bestShotPacket: bestShotPackets)
            pushMetadataPacket(bestShotPacket.releasePtr());
    }

    if (m_generateTitle)
    {
        generateTitleObject();

        std::vector<Ptr<IObjectTrackTitlePacket>> titlePackets = generateTitles();
        for (Ptr<IObjectTrackTitlePacket>& titlePacket: titlePackets)
            pushMetadataPacket(titlePacket.releasePtr());
    }
}

bool DeviceAgent::pushCompressedVideoFrame(const ICompressedVideoPacket* videoPacket)
{
    m_lastFrameTimestampUs = videoPacket->timestampUs();
    maybeGenerateBestShotAndTitle();
    return true;
}

void DeviceAgent::configureBestShots(std::map<std::string, std::string>& settings)
{
    nx::kit::utils::fromString(settings[kEnableBestShotGeneration], &m_generateBestShot);

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
        settings[kBestShotTopLeftXSetting],
        &m_bestShotGenerationContext.fixedBestShotBoundingBox.x);

    nx::kit::utils::fromString(
        settings[kBestShotTopLeftYSetting],
        &m_bestShotGenerationContext.fixedBestShotBoundingBox.y);

    nx::kit::utils::fromString(
        settings[kBestShotWidthSetting],
        &m_bestShotGenerationContext.fixedBestShotBoundingBox.width);

    nx::kit::utils::fromString(
        settings[kBestShotHeightSetting],
        &m_bestShotGenerationContext.fixedBestShotBoundingBox.height);

    nx::kit::utils::fromString(
        settings[kFrameNumberToGenerateBestShotSetting],
        &m_bestShotGenerationContext.frameNumberToGenerateBestShot);

    int bestShotObjectCount = 0;
    nx::kit::utils::fromString(settings[kBestShotObjectCountSetting], &bestShotObjectCount);
    m_bestShotTrackContexts.clear();

    for (int i = 0; i < bestShotObjectCount; ++i)
    {
        TrackContext trackContext;
        trackContext.trackId = UuidHelper::randomUuid();

        m_bestShotGenerationCounterByTrackId[trackContext.trackId] =
            m_bestShotGenerationContext.frameNumberToGenerateBestShot;

        trackContext.boundingBox.height = std::min(
            kMaxObjectHeight,
            (1.0F - kDistanceBetweenObjects * bestShotObjectCount) / bestShotObjectCount);
        trackContext.boundingBox.width = kObjectWidth;
        trackContext.boundingBox.x = 0.0F;
        trackContext.boundingBox.y = (i + 1) * kDistanceBetweenObjects
            + i * trackContext.boundingBox.height;

        m_bestShotTrackContexts.push_back(std::move(trackContext));
    }
}

void DeviceAgent::configureTitles(std::map<std::string, std::string>& settings)
{
    nx::kit::utils::fromString(settings[kEnableObjectTitleGeneration], &m_generateTitle);

    m_titleGenerationCounterByTrackId.clear();

    m_titleGenerationContext.policy = titleGenerationPolicyFromString(
        settings[kTitleGenerationPolicySetting]);

    const std::string titleImagePath = settings[kTitleImagePathSetting];
    if (!titleImagePath.empty())
    {
        m_titleGenerationContext.imageData = loadFile(titleImagePath);
        m_titleGenerationContext.imageDataFormat = imageFormatFromPath(titleImagePath);
    }

    m_titleGenerationContext.url = settings[kTitleUrlSetting];

    nx::kit::utils::fromString(
        settings[kTitleTopLeftXSetting],
        &m_titleGenerationContext.fixedTitleBoundingBox.x);

    nx::kit::utils::fromString(
        settings[kTitleTopLeftYSetting],
        &m_titleGenerationContext.fixedTitleBoundingBox.y);

    nx::kit::utils::fromString(
        settings[kTitleWidthSetting],
        &m_titleGenerationContext.fixedTitleBoundingBox.width);

    nx::kit::utils::fromString(
        settings[kTitleHeightSetting],
        &m_titleGenerationContext.fixedTitleBoundingBox.height);

    nx::kit::utils::fromString(
        settings[kFrameNumberToGenerateTitleSetting],
        &m_titleGenerationContext.frameNumberToGenerateTitle);

    int titleObjectCount = 0;
    nx::kit::utils::fromString(settings[kTitleObjectCountSetting], &titleObjectCount);
    m_titleTrackContexts.clear();

    for (int i = 0; i < titleObjectCount; ++i)
    {
        TrackContext trackContext;
        trackContext.trackId = UuidHelper::randomUuid();

        m_titleGenerationCounterByTrackId[trackContext.trackId] =
            m_titleGenerationContext.frameNumberToGenerateTitle;

        trackContext.boundingBox.height = std::min(
            kMaxObjectHeight,
            (1.0F - kDistanceBetweenObjects * titleObjectCount) / titleObjectCount);
        trackContext.boundingBox.width = kObjectWidth;
        trackContext.boundingBox.x = 0.0F;
        trackContext.boundingBox.y = (i + 1) * kDistanceBetweenObjects
            + i * trackContext.boundingBox.height;

        m_titleTrackContexts.push_back(std::move(trackContext));
    }

    m_titleGenerationContext.text = settings[kTitleTextSetting];

    if (m_titleGenerationContext.text.empty())
        m_titleGenerationContext.text = "Default Title";
}

nx::sdk::Result<const nx::sdk::ISettingsResponse*> DeviceAgent::settingsReceived()
{
    std::map<std::string, std::string> settings = currentSettings();

    configureBestShots(settings);
    configureTitles(settings);

    return nullptr;
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

    auto it = m_titleGenerationCounterByTrackId.begin();
    while (it != m_titleGenerationCounterByTrackId.end())
    {
        const Uuid& trackId = it->first;
        int& counter = it->second;

        if (counter == 0)
        {
            if (m_titleGenerationContext.policy == TitleGenerationPolicy::fixed)
                result.push_back(generateFixedTitle(trackId));
            else if (m_titleGenerationContext.policy == TitleGenerationPolicy::url)
                result.push_back(generateUrlTitle(trackId));
            else if (m_titleGenerationContext.policy == TitleGenerationPolicy::image)
                result.push_back(generateImageTitle(trackId));

            it = m_titleGenerationCounterByTrackId.erase(it);
        }
        else
        {
            --counter;
            ++it;
        }
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

Ptr<IObjectTrackTitlePacket> DeviceAgent::generateFixedTitle(Uuid trackId)
{
    return makePtr<ObjectTrackTitlePacket>(
        trackId,
        m_lastFrameTimestampUs,
        m_titleGenerationContext.fixedTitleBoundingBox,
        m_titleGenerationContext.text);
}

Ptr<IObjectTrackTitlePacket> DeviceAgent::generateUrlTitle(Uuid trackId)
{
    auto titlePacket = makePtr<ObjectTrackTitlePacket>(
        trackId,
        m_lastFrameTimestampUs);

    titlePacket->setText(m_titleGenerationContext.text);

    titlePacket->setImageUrl(m_titleGenerationContext.url);

    return titlePacket;
}

Ptr<IObjectTrackTitlePacket> DeviceAgent::generateImageTitle(Uuid trackId)
{
    auto titlePacket = makePtr<ObjectTrackTitlePacket>(
        trackId,
        m_lastFrameTimestampUs);

    titlePacket->setText(m_titleGenerationContext.text);

    titlePacket->setImageDataFormat(m_titleGenerationContext.imageDataFormat);
    titlePacket->setImageData(m_titleGenerationContext.imageData);

    return titlePacket;
}

} // namespace best_shots
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
