// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include "stream_parser.h"

#include <nx/sdk/analytics/helpers/consuming_device_agent.h>
#include <nx/sdk/analytics/i_object_metadata_packet.h>

#include <nx/kit/json.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_streamer {

class DeviceAgent: public nx::sdk::analytics::ConsumingDeviceAgent
{
public:
    DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo, std::string pluginHomeDir);
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
    std::vector<nx::sdk::Ptr<nx::sdk::analytics::IObjectMetadataPacket>> generateObjects(
        int frameNumber,
        int64_t frameTimestampUs,
        int64_t durationUs);

    sdk::Ptr<sdk::ISettingsResponse> makeSettingsResponse(
        const std::string& manifestFilePath,
        const std::string& streamFilePath) const;

    void reportIssues(const Issues& issues) const;

private:
    StreamInfo m_streamInfo;
    std::set<std::string> m_disabledObjectTypeIds;
    int m_frameNumber = 0;
    int m_maxFrameNumber = 0;
    int64_t m_lastFrameTimestampUs = -1;
    std::string m_pluginHomeDir;
    bool m_isInitialSettings = true;
};

} // namespace object_streamer
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
