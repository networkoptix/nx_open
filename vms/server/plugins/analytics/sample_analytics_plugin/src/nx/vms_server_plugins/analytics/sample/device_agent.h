// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/helpers/consuming_device_agent.h>
#include <nx/sdk/helpers/uuid_helper.h>

#include "engine.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace sample {

class DeviceAgent: public nx::sdk::analytics::ConsumingDeviceAgent
{
public:
    DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo);
    virtual ~DeviceAgent() override;

protected:
    virtual std::string manifestString() const override;

    virtual bool pushCompressedVideoFrame(
        const nx::sdk::analytics::ICompressedVideoPacket* videoFrame) override;

    virtual bool pullMetadataPackets(
        std::vector<nx::sdk::analytics::IMetadataPacket*>* metadataPackets) override;

    virtual void doSetNeededMetadataTypes(
        nx::sdk::Result<void>* outValue,
        const nx::sdk::analytics::IMetadataTypes* neededMetadataTypes) override;

private:
    nx::sdk::Ptr<nx::sdk::analytics::IMetadataPacket> generateEventMetadataPacket();
    nx::sdk::Ptr<nx::sdk::analytics::IMetadataPacket> generateObjectMetadataPacket();

private:
    const std::string kHelloWorldObjectType = "nx.sample.helloWorld";
    const std::string kNewTrackEventType = "nx.sample.newTrack";

    /** Lenght of the the track (in frames). The value was chosen arbitrarily. */
    static constexpr int kTrackFrameCount = 256;

private:
    nx::sdk::Uuid m_trackId = nx::sdk::UuidHelper::randomUuid();
    int m_frameIndex = 0; /**< Used for generating the detection in the right place. */
    int m_trackIndex = 0; /**< Used in the description of the events. */

    /** Used for binding object and event metadata to the particular video frame. */
    int64_t m_lastVideoFrameTimestampUs = 0;
};

} // namespace sample
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
