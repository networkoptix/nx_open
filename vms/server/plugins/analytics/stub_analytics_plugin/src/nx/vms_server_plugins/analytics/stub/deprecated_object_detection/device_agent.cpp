// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <vector>
#include <string>
#include <mutex>
#include <chrono>
#include <ctime>
#include <thread>
#include <type_traits>

#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>
#include <nx/kit/utils.h>
#include <nx/kit/json.h>

#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_track_best_shot_packet.h>
#include <nx/sdk/analytics/i_motion_metadata_packet.h>
#include <nx/sdk/helpers/error.h>
#include <nx/sdk/helpers/settings_response.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/helpers/uuid_helper.h>

#include "../utils.h"
#include "objects/bicycle.h"
#include "objects/vehicles.h"
#include "objects/pedestrian.h"
#include "objects/human_face.h"
#include "objects/stone.h"

#include "settings_model.h"
#include "stub_analytics_plugin_deprecated_object_detection_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace deprecated_object_detection {

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using namespace std::chrono;
using namespace std::literals::chrono_literals;

DeviceAgent::DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo):
    ConsumingDeviceAgent(deviceInfo, NX_DEBUG_ENABLE_OUTPUT, engine->plugin()->instanceId()),
    m_engine(engine)
{
}

DeviceAgent::~DeviceAgent()
{
}

/**
 * DeviceAgent manifest may declare eventTypes and objectTypes similarly to how an Engine declares
 * them - semantically the set from the Engine manifest is joined with the set from the DeviceAgent
 * manifest. Also this manifest should declare supportedEventTypeIds and supportedObjectTypeIds
 * lists which are treated as white-list filters for the respective set from Engine manifest
 * (absent lists are treated as empty lists, thus, disabling all types from the Engine).
 */
std::string DeviceAgent::manifestString() const
{
    std::string result = /*suppress newline*/ 1 + (const char*) R"json(
{
    "supportedObjectTypeIds":
    [
)json" + join(m_standardTaxonomyModule.supportedObjectTypeIds(), ", ", "\"", "\"");

    if (ini().declareStubObjectTypes)
    {
        result +=
R"json(,
        ")json" + kCarObjectType + R"json(",
        ")json" + kStoneObjectType + R"json(",
        ")json" + kHumanFaceObjectType + R"json(",
        ")json" + kTruckObjectType + R"json(",
        ")json" + kPedestrianObjectType + R"json(",
        ")json" + kBicycleObjectType + R"json("
)json";
    }

    result += 1 + R"json(
    ],
    "supportedTypes":
    [)json"
        + join(m_standardTaxonomyModule.supportedTypes(), ",") +
    R"json(
    ],
    "typeLibrary": {
        "objectTypes": [)json"
            + join(m_standardTaxonomyModule.typeLibraryObjectTypes(), ",");

    if (ini().declareStubObjectTypes)
    {
        result += R"json(,
            {
                "id": ")json" + kTruckObjectType + R"json(",
                "name": "Truck",
                "base": ")json" + kVehicleObjectType + R"json("
            },
            {
                "id": ")json" + kPedestrianObjectType + R"json(",
                "name": "Pedestrian"
            },
            {
                "id": ")json" + kBicycleObjectType + R"json(",
                "name": "Bicycle",
                "base": ")json" + kVehicleObjectType + R"json("
            })json";
    }

    result += R"json(
        ],
        "enumTypes": [)json" + join(m_standardTaxonomyModule.typeLibraryEnumTypes(), ",") + R"json(
        ],
        "colorTypes": [)json" + join(m_standardTaxonomyModule.typeLibraryColorTypes(), ",") + R"json(
        ]
    }
}
)json";

    return result;
}

Result<const ISettingsResponse*> DeviceAgent::settingsReceived()
{
    parseSettings();
    updateObjectGenerationParameters();
    pushManifest(manifestString());

    m_standardTaxonomyModule.setSettings(currentSettings());

    return nullptr;
}

/** @param func Name of the caller for logging; supply __func__. */
void DeviceAgent::processVideoFrame(const IDataPacket* videoFrame, const char* func)
{
    if (m_deviceAgentSettings.additionalFrameProcessingDelayMs.load()
        > std::chrono::milliseconds::zero())
    {
        std::this_thread::sleep_for(m_deviceAgentSettings.additionalFrameProcessingDelayMs.load());
    }

    NX_OUTPUT << func << "(): timestamp " << videoFrame->timestampUs() << " us;"
        << " frame #" << m_frameCounter;

    ++m_frameCounter;

    const int64_t frameTimestamp = videoFrame->timestampUs();
    m_lastVideoFrameTimestampUs = frameTimestamp;
    if (m_frameTimestampUsQueue.empty() || m_frameTimestampUsQueue.back() < frameTimestamp)
        m_frameTimestampUsQueue.push_back(frameTimestamp);
}

bool DeviceAgent::pushCompressedVideoFrame(const ICompressedVideoPacket* videoFrame)
{
    NX_OUTPUT << "Received compressed video frame, resolution: "
        << videoFrame->width() << "x" << videoFrame->height();

    processVideoFrame(videoFrame, __func__);
    return true;
}

bool DeviceAgent::pullMetadataPackets(std::vector<IMetadataPacket*>* metadataPackets)
{
    NX_OUTPUT << __func__ << "() BEGIN";

    if (!m_deviceAgentSettings.needToGenerateObjects()
        && !m_standardTaxonomyModule.needToGenerateObjects())
    {
        NX_OUTPUT << __func__ << "() END -> true: no need to generate object metadata packets";
        cleanUpTimestampQueue();
        return true;
    }

    *metadataPackets = cookSomeObjects();
    m_lastVideoFrameTimestampUs = 0;

    NX_OUTPUT << __func__ << "() END -> true: " <<
        nx::kit::utils::format("generated %d metadata packet(s)", metadataPackets->size());
    return true;
}

void DeviceAgent::doSetNeededMetadataTypes(
    Result<void>* /*outResult*/, const IMetadataTypes* neededMetadataTypes)
{
}

//-------------------------------------------------------------------------------------------------
// private

static Ptr<IObjectMetadata> makeObjectMetadata(const AbstractObject* object)
{
    auto objectMetadata = makePtr<ObjectMetadata>();
    objectMetadata->setTypeId(object->typeId());
    objectMetadata->setTrackId(object->id());
    const auto position = object->position();
    const auto size = object->size();
    objectMetadata->setBoundingBox(Rect(position.x, position.y, size.width, size.height));
    objectMetadata->addAttributes(object->attributes());
    return objectMetadata;
}

void DeviceAgent::setObjectCount(int objectCount)
{
    std::unique_lock<std::mutex> lock(m_objectGenerationMutex);

    if (objectCount < 1)
    {
        NX_OUTPUT << "Invalid value for objectCount: " << objectCount << ", assuming 1";
        objectCount = 1;
    }
    m_objectContexts.resize(objectCount);
}

void DeviceAgent::cleanUpTimestampQueue()
{
    std::unique_lock<std::mutex> lock(m_objectGenerationMutex);
    m_frameTimestampUsQueue.clear();
    m_lastVideoFrameTimestampUs = 0;
}

std::vector<IMetadataPacket*> DeviceAgent::cookSomeObjects()
{
    std::unique_lock<std::mutex> lock(m_objectGenerationMutex);

    std::vector<IMetadataPacket*> result;

    if (m_lastVideoFrameTimestampUs == 0)
        return result;

    if (m_frameTimestampUsQueue.empty())
        return result;

    const auto metadataTimestampUs = m_frameTimestampUsQueue.front();

    auto objectMetadataPacket = makePtr<ObjectMetadataPacket>();
    objectMetadataPacket->setTimestampUs(metadataTimestampUs);
    objectMetadataPacket->setDurationUs(0);

    const microseconds delay(m_lastVideoFrameTimestampUs - metadataTimestampUs);

    if (delay < m_deviceAgentSettings.overallMetadataDelayMs.load())
        return result;

    m_frameTimestampUsQueue.pop_front();

    if (m_frameCounter % m_deviceAgentSettings.generateObjectsEveryNFrames != 0)
        return result;

    for (auto& context: m_objectContexts)
    {
        if (!context)
        {
            if (m_standardTaxonomyModule.needToGenerateObjects()
                && (m_frameCounter % 2 == 0
                    || !m_deviceAgentSettings.needToGenerateObjects()
                    || !ini().declareStubObjectTypes))
            {
                context = m_standardTaxonomyModule.generateObject();
            }
            else if (m_deviceAgentSettings.needToGenerateObjects()
                && ini().declareStubObjectTypes)
            {
                context = m_objectGenerator.generate();
            }
        }

        if (!context)
            continue;

        const auto& object = context.object;
        object->update();

        if (!object->inBounds())
            context.reset();

        if (!object)
            continue;

        ++context.frameCounter;

        const bool previewIsNeeded = m_deviceAgentSettings.generatePreviews
            && context.frameCounter > m_deviceAgentSettings.numberOfFramesBeforePreviewGeneration
            && !context.isPreviewGenerated;

        if (previewIsNeeded)
        {
            const auto position = object->position();
            const auto size = object->size();
            auto bestShotPacket = new ObjectTrackBestShotPacket(
                object->id(),
                metadataTimestampUs,
                Rect(position.x, position.y, size.width, size.height));

            if (!m_deviceAgentSettings.previewImageUrl.empty())
            {
                bestShotPacket->setImageUrl(m_deviceAgentSettings.previewImageUrl);
            }
            else if (!m_deviceAgentSettings.previewImage.empty()
                && !m_deviceAgentSettings.previewImageFormat.empty())
            {
                bestShotPacket->setImage(
                    m_deviceAgentSettings.previewImageFormat,
                    m_deviceAgentSettings.previewImage);
            }

            result.push_back(bestShotPacket);
            context.isPreviewGenerated = true;
        }

        const auto objectMetadata = makeObjectMetadata(object.get());
        objectMetadataPacket->addItem(objectMetadata.get());
    }

    result.push_back(objectMetadataPacket.releasePtr());
    return result;
}

void DeviceAgent::parseSettings()
{
    auto assignNumericSetting =
        [this](
            const std::string& parameterName,
            auto target,
            std::function<void()> onChange = nullptr)
        {
            using namespace nx::kit::utils;
            using SettingType =
                std::conditional_t<
                    std::is_floating_point<decltype(target->load())>::value, float, int>;

            SettingType result{};
            const auto parameterValueString = settingValue(parameterName);
            if (fromString(parameterValueString, &result))
            {
                using AtomicValueType = decltype(target->load());
                if (target->load() != AtomicValueType{result})
                {
                    target->store(AtomicValueType{ result });
                    if (onChange)
                        onChange();
                }
            }
            else
            {
                NX_PRINT << "Received an incorrect setting value for '"
                    << parameterName << "': "
                    << nx::kit::utils::toString(parameterValueString)
                    << ". Expected an integer.";
            }
        };

    m_deviceAgentSettings.generateCars = toBool(settingValue(kGenerateCarsSetting));
    m_deviceAgentSettings.generateTrucks = toBool(settingValue(kGenerateTrucksSetting));
    m_deviceAgentSettings.generatePedestrians = toBool(settingValue(kGeneratePedestriansSetting));
    m_deviceAgentSettings.generateHumanFaces = toBool(settingValue(kGenerateHumanFacesSetting));
    m_deviceAgentSettings.generateBicycles = toBool(settingValue(kGenerateBicyclesSetting));
    m_deviceAgentSettings.generateStones = toBool(settingValue(kGenerateStonesSetting));

    m_deviceAgentSettings.generatePreviews = toBool(settingValue(kGeneratePreviewPacketSetting));

    assignNumericSetting(
        kGenerateObjectsEveryNFramesSetting,
        &m_deviceAgentSettings.generateObjectsEveryNFrames);

    assignNumericSetting(
        kNumberOfObjectsToGenerateSetting,
        &m_deviceAgentSettings.numberOfObjectsToGenerate);

    assignNumericSetting(
        kAdditionalFrameProcessingDelayMsSetting,
        &m_deviceAgentSettings.additionalFrameProcessingDelayMs);

    assignNumericSetting(
        kOverallMetadataDelayMsSetting,
        &m_deviceAgentSettings.overallMetadataDelayMs,
        [this]() { cleanUpTimestampQueue(); });

    assignNumericSetting(
        kGeneratePreviewAfterNFramesSetting,
        &m_deviceAgentSettings.numberOfFramesBeforePreviewGeneration);

    std::unique_lock<std::mutex> lock(m_objectGenerationMutex);

    const std::string previewPath = settingValue(kPreviewImageFileSetting);
    m_deviceAgentSettings.previewImageUrl = std::string();

    if (isHttpOrHttpsUrl(previewPath))
    {
        m_deviceAgentSettings.previewImageUrl = previewPath;
    }
    else
    {
        m_deviceAgentSettings.previewImageFormat = imageFormatFromPath(previewPath);
        m_deviceAgentSettings.previewImage = m_deviceAgentSettings.previewImageFormat.empty()
            ? std::vector<char>()
            : loadFile(previewPath);

        if (m_deviceAgentSettings.previewImage.empty())
            m_deviceAgentSettings.previewImageFormat = std::string();
    }
}

void DeviceAgent::updateObjectGenerationParameters()
{
    setObjectCount(m_deviceAgentSettings.numberOfObjectsToGenerate);
    setIsObjectTypeGenerationNeeded<Car>(m_deviceAgentSettings.generateCars);
    setIsObjectTypeGenerationNeeded<Truck>(m_deviceAgentSettings.generateTrucks);
    setIsObjectTypeGenerationNeeded<Pedestrian>(m_deviceAgentSettings.generatePedestrians);
    setIsObjectTypeGenerationNeeded<HumanFace>(m_deviceAgentSettings.generateHumanFaces);
    setIsObjectTypeGenerationNeeded<Bicycle>(m_deviceAgentSettings.generateBicycles);
    setIsObjectTypeGenerationNeeded<Stone>(m_deviceAgentSettings.generateStones);
}

} // namespace deprecated_object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
