// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/helpers/consuming_device_agent.h>
#include <nx/sdk/analytics/i_object_metadata_packet.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_actions {

class DeviceAgent: public nx::sdk::analytics::ConsumingDeviceAgent
{
public:
    DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo);
    virtual ~DeviceAgent() override;

protected:
    virtual std::string manifestString() const override;

    virtual bool pushCompressedVideoFrame(
        const nx::sdk::analytics::ICompressedVideoPacket* videoFrame) override;

    virtual void doSetNeededMetadataTypes(
        nx::sdk::Result<void>* outValue,
        const nx::sdk::analytics::IMetadataTypes* neededMetadataTypes) override;

private:
    struct TrackContext
    {
        nx::sdk::Uuid trackId;
        nx::sdk::analytics::Rect boundingBox;
    };

private:
    nx::sdk::Ptr<nx::sdk::analytics::IObjectMetadataPacket> generateObject(int64_t timestampUs);

private:
    TrackContext m_trackContext;
};

} // namespace object_actions
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
