// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/helpers/video_frame_processing_device_agent.h>
#include <nx/sdk/helpers/uuid_helper.h>

#include "engine.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace sample {

class DeviceAgent: public nx::sdk::analytics::VideoFrameProcessingDeviceAgent
{
public:
    DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo);
    virtual ~DeviceAgent() override;

protected:
    virtual std::string manifestString() const override;

    virtual bool pushUncompressedVideoFrame(
        const nx::sdk::analytics::IUncompressedVideoFrame* videoFrame) override;

    virtual bool pullMetadataPackets(
        std::vector<nx::sdk::analytics::IMetadataPacket*>* metadataPackets) override;

    virtual void doSetNeededMetadataTypes(
        nx::sdk::Result<void>* /*outValue*/,
        const nx::sdk::analytics::IMetadataTypes* /*neededMetadataTypes*/) override
    {
    }

private:
    nx::sdk::Ptr<sdk::analytics::IMetadataPacket> generateEventMetadataPacket();
    nx::sdk::Ptr<sdk::analytics::IMetadataPacket> generateObjectMetadataPacket();

private:
    const std::string kHelloWorldObjectType = "nx.sample.helloWorld";
    const std::string kNewTrackEventType = "nx.sample.newTrack";

    static constexpr int kTrackFrameCount = 256; //< The value was chosen arbitrarily.

private:
    nx::sdk::Uuid m_trackId = nx::sdk::UuidHelper::randomUuid();
    int m_frameIndex = 0;
    int m_trackIndex = 0;
    int64_t m_lastVideoFrameTimestampUs = 0;
};

} // namespace sample
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
