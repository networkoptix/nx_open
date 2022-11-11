// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>
#include <thread>

#include <nx/sdk/analytics/helpers/consuming_device_agent.h>
#include <nx/sdk/helpers/uuid_helper.h>

#include "engine.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_detection {

class DeviceAgent: public nx::sdk::analytics::ConsumingDeviceAgent
{
public:
    static const std::string kTimeShiftSetting;
    static const std::string kSendAttributesSetting;
    static const std::string kObjectTypeGenerationSettingPrefix;

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

    virtual nx::sdk::Result<const nx::sdk::ISettingsResponse*> settingsReceived() override;

private:
    nx::sdk::Uuid trackIdByTrackIndex(int trackIndex);

    nx::sdk::Ptr<nx::sdk::analytics::IMetadataPacket> generateObjectMetadataPacket(
        int64_t frameTimestampUs);

private:
    mutable std::mutex m_mutex;

    int m_frameIndex = 0;
    int m_timestampShiftMs = 0;
    bool m_sendAttributes = true;
    std::vector<nx::sdk::Uuid> m_trackIds;
    std::set<std::string> m_objectTypeIdsToGenerate;
};

} // namespace object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
