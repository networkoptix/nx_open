// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <nx/kit/json.h>
#include <nx/sdk/analytics/helpers/consuming_device_agent.h>
#include <nx/sdk/analytics/i_object_metadata_packet.h>

#include "stream_parser.h"

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
    std::vector<nx::sdk::Ptr<nx::sdk::analytics::IMetadataPacket>> generateMetadata(
        int frameNumber,
        int64_t frameTimestampUs,
        int64_t durationUs);

    nx::sdk::Uuid obtainObjectTrackIdFromRef(const std::string& objectTrackIdRef);

    sdk::Ptr<sdk::ISettingsResponse> makeSettingsResponse(
        const std::string& manifestFilePath,
        const std::string& streamFilePath) const;

    void reportIssues(const Issues& issues) const;

private:
    StreamInfo m_streamInfo;
    std::set<std::string> m_disabledObjectTypeIds;
    int m_frameNumber = 0;
    int m_maxFrameNumber = 0;
    std::map<std::string, nx::sdk::Uuid> m_trackIdByRef;

    int64_t m_lastFrameTimestampUs = -1;
    std::string m_pluginHomeDir;
    bool m_isInitialSettings = true;
};

} // namespace object_streamer
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
