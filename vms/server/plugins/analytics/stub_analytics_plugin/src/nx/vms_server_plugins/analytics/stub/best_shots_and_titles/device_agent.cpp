// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <cmath>
#include <filesystem>
#include <numeric>
#include <random>

#include <nx/kit/debug.h>
#include <nx/kit/utils.h>
#include <nx/sdk/helpers/uuid_helper.h>

#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_track_best_shot_packet.h>
#include <nx/sdk/analytics/helpers/object_track_title_packet.h>
#include <nx/vms_server_plugins/analytics/stub/utils.h>

#include "settings.h"
#include "stub_analytics_plugin_best_shots_ini.h"

static const int kVectorSize = 768;

namespace fs = std::filesystem;

namespace {

bool ends_with(const std::string& value, std::string_view ending)
{
    if (ending.size() > value.size())
        return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

bool isLittleEndian()
{
    uint16_t num = 0x1;
    char* ptr = reinterpret_cast<char*>(&num);
    return ptr[0] == char(0x1);
}

} //namespace

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

        objectMetadataPacket->addItem(objectMetadata);
    }

    pushMetadataPacket(objectMetadataPacket);
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

        objectMetadataPacket->addItem(objectMetadata);
    }

    pushMetadataPacket(objectMetadataPacket);
}

void DeviceAgent::maybeGenerateBestShotAndTitle()
{
    if (m_generateBestShot)
    {
        generateBestShotObject();

        auto bestShotPackets = generateBestShots();
        for (auto& bestShotPacket: bestShotPackets)
            pushMetadataPacket(bestShotPacket);
    }

    if (m_generateTitle)
    {
        generateTitleObject();

        std::vector<Ptr<IObjectTrackTitlePacket>> titlePackets = generateTitles();
        for (Ptr<IObjectTrackTitlePacket>& titlePacket: titlePackets)
            pushMetadataPacket(titlePacket);
    }
}

bool DeviceAgent::pushCompressedVideoFrame(Ptr<const ICompressedVideoPacket> videoPacket)
{
    m_lastFrameTimestampUs = videoPacket->timestampUs();
    maybeGenerateBestShotAndTitle();
    return true;
}

std::vector<DeviceAgent::ImageData> DeviceAgent::loadImages(const std::string& path)
{
    std::vector<DeviceAgent::ImageData> result;

    if (!fs::exists(path))
    {
        NX_OUTPUT << __func__ << "Path does not exist: " << path;
        return result;
    }

    if (fs::is_regular_file(path))
    {
        // Load a single image file.
        DeviceAgent::ImageData imageData;
        imageData.data = loadFile(path);
        imageData.format = imageFormatFromPath(path);
        result.push_back(imageData);
    }
    else if (fs::is_directory(path))
    {
        // Load all image files from the directory.
        for (const auto& entry : fs::directory_iterator(path))
        {
            if (fs::is_regular_file(entry))
            {
                std::string filePath = entry.path().string();
                const bool isImage = ends_with(filePath, ".jpeg") || ends_with(filePath, ".jpg")
                    || ends_with(filePath, ".png") || ends_with(filePath, ".bmp");
                if (!isImage)
                    continue;

                DeviceAgent::ImageData imageData;
                imageData.data = loadFile(filePath);
                imageData.format = imageFormatFromPath(filePath);
                result.push_back(imageData);
            }
        }
    }
    return result;
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
        m_bestShotGenerationContext.images = loadImages(bestShotImagePath);
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

    nx::kit::utils::fromString(
        settings[kGenerateVectorSetting],
        &m_bestShotGenerationContext.generateVector);

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
        m_titleGenerationContext.images = loadImages(titleImagePath);
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

std::vector<char> generateRandomVector()
{
    std::vector<float> data(kVectorSize);

    // Fill with random values (example: uniform distribution)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (float& v: data)
        v = dist(gen);

    // Normalize to unit vector length (L2 norm = 1)
    float norm = std::sqrt(std::accumulate(data.begin(), data.end(), 0.0f,
        [](float sum, float val) { return sum + val * val; }));

    if (norm > 0.0f)
    {
        for (float& v: data)
            v /= norm;
    }

    // Write to QByteArray via QDataStream in Little Endian
    std::vector<char> output;
    output.reserve(kVectorSize * sizeof(float));
    if (!isLittleEndian())
    {
        for (float v: data)
        {
            uint32_t asInt;
            std::memcpy(&asInt, &v, sizeof(float));

            // Convert to little endian manually (portable)
            char bytes[4];
            bytes[0] = asInt & 0xFF;
            bytes[1] = (asInt >> 8) & 0xFF;
            bytes[2] = (asInt >> 16) & 0xFF;
            bytes[3] = (asInt >> 24) & 0xFF;
            output.insert(output.end(), bytes, bytes + 4);
        }
    }
    else
    {
        const char* raw = reinterpret_cast<const char*>(data.data());
        output.insert(output.end(), raw, raw + kVectorSize * sizeof(float));
    }

    return output;
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
            Ptr<ObjectTrackBestShotPacket> bestShot;
            if (m_bestShotGenerationContext.policy == BestShotGenerationPolicy::fixed)
                bestShot = generateFixedBestShot(trackId);
            else if (m_bestShotGenerationContext.policy == BestShotGenerationPolicy::url)
                bestShot = generateUrlBestShot(trackId);
            else if (m_bestShotGenerationContext.policy == BestShotGenerationPolicy::image)
                bestShot = generateImageBestShot(trackId);

            if (bestShot)
            {
                if (m_bestShotGenerationContext.generateVector)
                    bestShot->setVectorData(generateRandomVector());
                result.push_back(std::move(bestShot));
            }

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

Ptr<ObjectTrackBestShotPacket> DeviceAgent::generateFixedBestShot(Uuid trackId)
{
    return makePtr<ObjectTrackBestShotPacket>(
        trackId,
        m_lastFrameTimestampUs,
        m_bestShotGenerationContext.fixedBestShotBoundingBox);
}

Ptr<ObjectTrackBestShotPacket> DeviceAgent::generateUrlBestShot(Uuid trackId)
{
    auto bestShotPacket = makePtr<ObjectTrackBestShotPacket>(
        trackId,
        m_lastFrameTimestampUs);

    bestShotPacket->setImageUrl(m_bestShotGenerationContext.url);

    return bestShotPacket;
}

Ptr<ObjectTrackBestShotPacket> DeviceAgent::generateImageBestShot(Uuid trackId)
{
    auto bestShotPacket = makePtr<ObjectTrackBestShotPacket>(
        trackId,
        m_lastFrameTimestampUs);

    if (!m_bestShotGenerationContext.images.empty())
    {
        int index = rand() % m_bestShotGenerationContext.images.size();
        bestShotPacket->setImageDataFormat(m_bestShotGenerationContext.images[index].format);
        bestShotPacket->setImageData(m_bestShotGenerationContext.images[index].data);
    }

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

    if (!m_titleGenerationContext.images.empty())
    {
        int index = rand() % m_titleGenerationContext.images.size();
        titlePacket->setImageDataFormat(m_titleGenerationContext.images[index].format);
        titlePacket->setImageData(m_titleGenerationContext.images[index].data);
    }

    return titlePacket;
}

} // namespace best_shots
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
