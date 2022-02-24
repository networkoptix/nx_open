// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <chrono>

#include <nx/sdk/analytics/helpers/consuming_device_agent.h>
#include <nx/sdk/analytics/i_motion_metadata_packet.h>

#include "engine.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace motion_metadata {

const std::string kMotionVisualizationObjectType{"nx.stub.motionVisualization"};
const std::string kAdditionalFrameProcessingDelayMsSetting{"additionalFrameProcessingDelayMs"};
const std::string kObjectWidthInMotionCellsSetting{"objectWidthInMotionCells"};
const std::string kObjectHeightInMotionCellsSetting{"objectHeightInMotionCells"};

class DeviceAgent: public nx::sdk::analytics::ConsumingDeviceAgent
{
public:
    DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo);
    virtual ~DeviceAgent() override;

protected:
    virtual void doSetNeededMetadataTypes(
        nx::sdk::Result<void>* outValue,
        const nx::sdk::analytics::IMetadataTypes* neededMetadataTypes) override;

    virtual std::string manifestString() const override;

    virtual nx::sdk::Result<const nx::sdk::ISettingsResponse*> settingsReceived() override;

    virtual bool pushCompressedVideoFrame(
        const nx::sdk::analytics::ICompressedVideoPacket* videoFrame) override;

private:
    void processVideoFrame(const nx::sdk::analytics::IDataPacket* videoFrame, const char* func);

    void processFrameMotion(
        nx::sdk::Ptr<nx::sdk::IList<nx::sdk::analytics::IMetadataPacket>> metadataPacketList);

    bool hasMotionUnderObject(
        int objectColumn,
        int objectRow,
        nx::sdk::Ptr<nx::sdk::analytics::IMotionMetadataPacket> motionMetadataPacket);

private:
    Engine* const m_engine;

    int m_frameCounter = 0;

    struct DeviceAgentSettings
    {
        std::atomic<int> objectWidthInMotionCells{8};
        std::atomic<int> objectHeightInMotionCells{8};

        std::atomic<std::chrono::milliseconds> additionalFrameProcessingDelayMs{
            std::chrono::milliseconds::zero()};
    };

    DeviceAgentSettings m_deviceAgentSettings;
    std::vector<nx::sdk::Uuid> m_objectTrackIdForObjectCells;
};

} // namespace motion_metadata
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
