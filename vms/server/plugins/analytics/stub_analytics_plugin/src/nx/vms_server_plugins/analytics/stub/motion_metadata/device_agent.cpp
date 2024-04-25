// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <thread>
#include <algorithm>

#include <nx/kit/utils.h>
#include <nx/kit/json.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_track_best_shot_packet.h>
#include <nx/sdk/helpers/error.h>
#include <nx/sdk/helpers/settings_response.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/helpers/uuid_helper.h>

#include "../utils.h"

#include "stub_analytics_plugin_motion_metadata_ini.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace motion_metadata {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

DeviceAgent::DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo):
    ConsumingDeviceAgent(deviceInfo, NX_DEBUG_ENABLE_OUTPUT, engine->plugin()->instanceId()),
    m_engine(engine)
{
}

DeviceAgent::~DeviceAgent()
{
}

std::string DeviceAgent::manifestString() const
{
    std::string result = /*suppress newline*/ 1 + (const char*)R"json(
{
    "supportedTypes":
    [
        {
            "objectTypeId": ")json" + kMotionVisualizationObjectType + R"json("
        }
    ]
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

            // TODO: Limit to 32x32.
            SettingType result{};
            const auto parameterValueString = settingValue(parameterName);
            if (fromString(parameterValueString, &result))
            {
                using AtomicValueType = decltype(target->load());
                if (target->load() != AtomicValueType{result})
                {
                    target->store(AtomicValueType{result});
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

    assignNumericSetting(
        kObjectWidthInMotionCellsSetting,
        &m_deviceAgentSettings.objectWidthInMotionCells);

    assignNumericSetting(
        kObjectHeightInMotionCellsSetting,
        &m_deviceAgentSettings.objectHeightInMotionCells);

    assignNumericSetting(
        kAdditionalFrameProcessingDelayMsSetting,
        &m_deviceAgentSettings.additionalFrameProcessingDelayMs);

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
}

bool DeviceAgent::pushCompressedVideoFrame(const ICompressedVideoPacket* videoFrame)
{
    NX_OUTPUT << "Received compressed video frame, resolution: "
        << videoFrame->width() << "x" << videoFrame->height();

    processVideoFrame(videoFrame, __func__);
    processFrameMotion(videoFrame->metadataList());
    return true;
}

bool DeviceAgent::hasMotionUnderObject(
    int objectColumn, int objectRow, Ptr<IMotionMetadataPacket> motionMetadataPacket)
{
    for (int column = objectColumn * m_deviceAgentSettings.objectWidthInMotionCells;
        column < std::min(
            (objectColumn + 1) * m_deviceAgentSettings.objectWidthInMotionCells,
            motionMetadataPacket->columnCount());
        ++column)
    {
        for (int row = objectRow * m_deviceAgentSettings.objectHeightInMotionCells;
            row < std::min(
                (objectRow + 1) * m_deviceAgentSettings.objectHeightInMotionCells,
                motionMetadataPacket->rowCount());
            ++row)
        {
            if (motionMetadataPacket->isMotionAt(column, row))
                return true;
        }
    }

    return false;
}

void DeviceAgent::processFrameMotion(Ptr<IList<IMetadataPacket>> metadataPacketList)
{
    if (!metadataPacketList)
        return;

    const int metadataPacketCount = metadataPacketList->count();

    NX_OUTPUT << "Received " << metadataPacketCount << " metadata packet(s) with the frame.";

    if (metadataPacketCount == 0)
        return;

    int motionObjectMetadataCount = 0;

    for (int i = 0; i < metadataPacketCount; ++i)
    {
        const auto metadataPacket = metadataPacketList->at(i);
        if (!NX_KIT_ASSERT(metadataPacket))
            continue;

        const auto motionPacket = metadataPacket->queryInterface<IMotionMetadataPacket>();
        if (!motionPacket)
            continue;

        auto objectMetadataPacket = makePtr<ObjectMetadataPacket>();
        objectMetadataPacket->setTimestampUs(motionPacket->timestampUs());

        int objectColumnCount =
            motionPacket->columnCount() / m_deviceAgentSettings.objectWidthInMotionCells;
        if (objectColumnCount < 1)
            objectColumnCount = 1;
        int objectRowCount =
            motionPacket->rowCount() / m_deviceAgentSettings.objectHeightInMotionCells;
        if (objectRowCount < 1)
            objectRowCount = 1;
        if (m_objectTrackIdForObjectCells.size() != objectColumnCount * objectRowCount)
        {
            m_objectTrackIdForObjectCells.resize(objectColumnCount * objectRowCount);
            for (auto& objectTrackId: m_objectTrackIdForObjectCells)
                objectTrackId = UuidHelper::randomUuid();
        }

        for (int objectColumn = 0; objectColumn < objectColumnCount; ++objectColumn)
        {
            for (int objectRow = 0; objectRow < objectRowCount; ++objectRow)
            {
                if (!hasMotionUnderObject(objectColumn, objectRow, motionPacket))
                    continue;

                const auto objectMetadata = makePtr<ObjectMetadata>();
                objectMetadata->setBoundingBox(Rect(
                    objectColumn / (float) objectColumnCount,
                    objectRow / (float) objectRowCount,
                    1.0F / objectColumnCount,
                    1.0F / objectRowCount));

                objectMetadata->setTypeId(kMotionVisualizationObjectType);
                objectMetadata->setTrackId(
                    m_objectTrackIdForObjectCells[objectColumn * objectRowCount + objectRow]);
                objectMetadata->setConfidence(1.0F);
                objectMetadataPacket->addItem(objectMetadata.get());
            }
        }

        motionObjectMetadataCount += objectMetadataPacket->count();
        pushMetadataPacket(objectMetadataPacket.releasePtr());
    }

    NX_OUTPUT << "Generated " << motionObjectMetadataCount << " motion Objects for the frame.";
}

void DeviceAgent::doSetNeededMetadataTypes(
    Result<void>* /*outResult*/, const IMetadataTypes* /*neededMetadataTypes*/)
{
}

} // namespace motion_metadata
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
