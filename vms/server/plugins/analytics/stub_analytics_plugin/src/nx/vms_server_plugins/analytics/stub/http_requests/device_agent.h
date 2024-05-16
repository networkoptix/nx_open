// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/sdk/analytics/helpers/consuming_device_agent.h>
#include <nx/sdk/analytics/i_object_metadata_packet.h>

#include "engine.h"

namespace nx::vms_server_plugins::analytics::stub::http_requests {

class DeviceAgent: public nx::sdk::analytics::ConsumingDeviceAgent
{
public:
    DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo);
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
    struct HttpRequestContext
    {
        nx::sdk::IUtilityProvider4::HttpDomainName domain =
            nx::sdk::IUtilityProvider4::HttpDomainName::vms;
        std::string url = "/rest/v1/system/settings";
        std::string httpMethod = "GET";
        std::string mimeType;
        std::string requestBody;
        int64_t periodSeconds = 1;
    };

private:
    int64_t m_lastFrameTimestampUs = 0;
    int64_t m_lastEventTimestampUs = 0;
    HttpRequestContext m_requestContext;
    Engine* const m_engine;
};

} // namespace nx::vms_server_plugins::analytics::stub::http_requests
