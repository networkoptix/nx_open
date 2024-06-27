// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <vector>
#include <string>
#include <mutex>
#include <chrono>
#include <ctime>
#include <thread>
#include <type_traits>

#include <nx/kit/json.h>
#include <nx/kit/utils.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/helpers/error.h>
#include <nx/sdk/helpers/settings_response.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/helpers/uuid_helper.h>

#include "../utils.h"

#include "settings_model.h"
#include "stub_analytics_plugin_special_objects_ini.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace special_objects {

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using namespace std::chrono;
using namespace std::literals::chrono_literals;
using Uuid = nx::sdk::Uuid;

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
 * manifest. Also this manifest should declare the supportedTypes list which is treated as a
 * white-list filter for any type; an absent list is treated as an empty list, thus, disabling all
 * types.
 */
std::string DeviceAgent::manifestString() const
{
    std::string result = /*suppress newline*/ 1 + (const char*)R"json(
{
    "supportedTypes":
    [
        { "objectTypeId": ")json" + kFixedObjectType + R"json(" },
        { "objectTypeId": ")json" + kBlinkingObjectType + R"json(" },
        { "objectTypeId": ")json" + kCounterObjectType + R"json(" }
    ],
    "typeLibrary":
    {
        "objectTypes":
        [
            {
                "id": ")json" + kBlinkingObjectType + R"json(",
                "name": "Blinking Object"
            },
            {
                "id": ")json" + kCounterObjectType + R"json(",
                "name": "Counter",
                "flags": "nonIndexable"
            }
        ]
    }
}
)json";

    return result;
}

Result<const ISettingsResponse*> DeviceAgent::settingsReceived()
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

    m_deviceAgentSettings.generateFixedObject = toBool(settingValue(kGenerateFixedObjectSetting));

    {
        std::unique_lock<std::mutex> lock(m_deviceAgentSettings.fixedObjectColorMutex);
        m_deviceAgentSettings.fixedObjectColor = settingValue(kFixedObjectColorSetting);
    }

    m_deviceAgentSettings.generateCounter = toBool(settingValue(kGenerateCounterSetting));

    assignNumericSetting(kBlinkingObjectPeriodMsSetting,
        &m_deviceAgentSettings.blinkingObjectPeriodMs);

    m_deviceAgentSettings.blinkingObjectInDedicatedPacket =
        toBool(settingValue(kBlinkingObjectInDedicatedPacketSetting));

    assignNumericSetting(
        kGenerateObjectsEveryNFramesSetting,
        &m_deviceAgentSettings.generateObjectsEveryNFrames);

    assignNumericSetting(
        kAdditionalFrameProcessingDelayMsSetting,
        &m_deviceAgentSettings.additionalFrameProcessingDelayMs);

    assignNumericSetting(
        kOverallMetadataDelayMsSetting,
        &m_deviceAgentSettings.overallMetadataDelayMs,
        [this]() { cleanUpTimestampQueue(); });

    assignNumericSetting(
        kCounterBoundingBoxSideSizeSetting,
        &m_deviceAgentSettings.counterBoundingBoxSideSize);

    assignNumericSetting(
        kCounterXOffsetSetting,
        &m_deviceAgentSettings.counterBoundingBoxXOffset);

    assignNumericSetting(
        kCounterYOffsetSetting,
        &m_deviceAgentSettings.counterBoundingBoxYOffset);

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

    if (!m_deviceAgentSettings.needToGenerateObjects())
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
    Result<void>* /*outResult*/, const IMetadataTypes* /*neededMetadataTypes*/)
{
}

//-------------------------------------------------------------------------------------------------
// private

void DeviceAgent::cleanUpTimestampQueue()
{
    std::unique_lock<std::mutex> lock(m_objectGenerationMutex);
    m_frameTimestampUsQueue.clear();
    m_lastVideoFrameTimestampUs = 0;
}

Ptr<IObjectMetadata> DeviceAgent::cookBlinkingObjectIfNeeded(int64_t metadataTimestampUs)
{
    const int64_t blinkingObjectPeriodUs =
        m_deviceAgentSettings.blinkingObjectPeriodMs.load().count() * 1000;

    if (blinkingObjectPeriodUs == 0)
        return nullptr;

    if (m_lastBlinkingObjectTimestampUs != 0 //< Not first time.
        && metadataTimestampUs - m_lastBlinkingObjectTimestampUs < blinkingObjectPeriodUs)
    {
        return nullptr;
    }

    m_lastBlinkingObjectTimestampUs = metadataTimestampUs;

    auto objectMetadata = makePtr<ObjectMetadata>();
    static const Uuid trackId = UuidHelper::randomUuid();
    objectMetadata->setTypeId(kBlinkingObjectType);
    objectMetadata->setTrackId(trackId);
    objectMetadata->setBoundingBox(Rect(0.25, 0.25, 0.5, 0.5));

    return objectMetadata;
}

/**
 * Cooks an object and either packs it into a new metadata packet inserting it into the given list,
 * or adds it to the existing metadata packet - depending on the settings. This is needed to test
 * the ability of the Server to receive multiple metadata packets.
 */
void DeviceAgent::addBlinkingObjectIfNeeded(
    int64_t metadataTimestampUs,
    std::vector<IMetadataPacket*>* metadataPackets,
    Ptr<ObjectMetadataPacket> objectMetadataPacket)
{
    const auto blinkingObjectMetadata = cookBlinkingObjectIfNeeded(metadataTimestampUs);
    if (!blinkingObjectMetadata)
        return;

    if (m_deviceAgentSettings.blinkingObjectInDedicatedPacket)
    {
        if (!NX_KIT_ASSERT(metadataPackets))
            return;

        auto dedicatedMetadataPacket = makePtr<ObjectMetadataPacket>();
        dedicatedMetadataPacket->setTimestampUs(metadataTimestampUs);
        dedicatedMetadataPacket->setDurationUs(0);

        dedicatedMetadataPacket->addItem(blinkingObjectMetadata.get());
        metadataPackets->push_back(dedicatedMetadataPacket.releasePtr());
    }
    else
    {
        objectMetadataPacket->addItem(blinkingObjectMetadata.get());
    }
}

void DeviceAgent::addFixedObjectIfNeeded(Ptr<ObjectMetadataPacket> objectMetadataPacket)
{
    if (!m_deviceAgentSettings.generateFixedObject)
        return;

    auto objectMetadata = makePtr<ObjectMetadata>();
    static const Uuid trackId = UuidHelper::randomUuid();
    objectMetadata->setTypeId(kFixedObjectType);
    objectMetadata->setTrackId(trackId);
    objectMetadata->setBoundingBox(Rect(0.1F, 0.1F, 0.25F, 0.25F));

    std::string fixedObjectColor;
    {
        std::unique_lock<std::mutex> lock(m_deviceAgentSettings.fixedObjectColorMutex);
        fixedObjectColor = m_deviceAgentSettings.fixedObjectColor;
    }

    if (fixedObjectColor != kNoSpecialColorSettingValue)
    {
        objectMetadata->addAttribute(makePtr<Attribute>(
            Attribute::Type::string, "nx.sys.color", fixedObjectColor));
    }

    objectMetadataPacket->addItem(objectMetadata.get());
}

void DeviceAgent::addCounterIfNeeded(Ptr<ObjectMetadataPacket> objectMetadataPacket)
{
    if (!m_deviceAgentSettings.generateCounter)
        return;

    auto objectMetadata = makePtr<ObjectMetadata>();
    static const Uuid trackId = UuidHelper::randomUuid();
    objectMetadata->setTypeId(kCounterObjectType);
    objectMetadata->setTrackId(trackId);

    const float realXOffset =
        clamp(m_deviceAgentSettings.counterBoundingBoxXOffset.load(), 0.0F, 1.0F);

    const float realYOffset =
        clamp(m_deviceAgentSettings.counterBoundingBoxYOffset.load(), 0.0F, 1.0F);

    const float realWidth =
        clamp(m_deviceAgentSettings.counterBoundingBoxSideSize.load(), 0.0F, 1.0F - realXOffset);

    const float realHeight =
        clamp(m_deviceAgentSettings.counterBoundingBoxSideSize.load(), 0.0F, 1.0F - realYOffset);

    objectMetadata->setBoundingBox(Rect(realXOffset, realYOffset, realWidth, realHeight));
    objectMetadata->addAttribute(makePtr<Attribute>(
        IAttribute::Type::number,
        "counterValue",
        std::to_string(m_counterObjectAttributeValue++)));

    objectMetadataPacket->addItem(objectMetadata.get());
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

    addBlinkingObjectIfNeeded(metadataTimestampUs, &result, objectMetadataPacket);
    addFixedObjectIfNeeded(objectMetadataPacket);
    addCounterIfNeeded(objectMetadataPacket);

    const microseconds delay(m_lastVideoFrameTimestampUs - metadataTimestampUs);

    if (delay < m_deviceAgentSettings.overallMetadataDelayMs.load())
        return result;

    m_frameTimestampUsQueue.pop_front();

    if (m_frameCounter % m_deviceAgentSettings.generateObjectsEveryNFrames != 0)
        return result;

    result.push_back(objectMetadataPacket.releasePtr());
    return result;
}

} // namespace special_objects
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
