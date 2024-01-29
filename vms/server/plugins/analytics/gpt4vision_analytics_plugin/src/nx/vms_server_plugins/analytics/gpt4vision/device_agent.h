// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/helpers/consuming_device_agent.h>
#include <nx/sdk/helpers/uuid_helper.h>

#include "openai_interface.h"

namespace nx::vms_server_plugins::analytics::gpt4vision {

using namespace std::chrono_literals;

class Engine;

class DeviceAgent: public nx::sdk::analytics::ConsumingDeviceAgent
{
public:
    DeviceAgent(const Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo);
    virtual ~DeviceAgent() override;

public:
    static constexpr std::string_view kQueryParameter = "query";
    static constexpr std::string_view kPeriodParameter = "period";
    static constexpr auto kDefaultQueryPeriod = 15s;

protected:
    virtual std::string manifestString() const override;

    virtual bool pushUncompressedVideoFrame(
        const nx::sdk::analytics::IUncompressedVideoFrame* videoFrame) override;

    virtual void doSetNeededMetadataTypes(
        nx::sdk::Result<void>* outValue,
        const nx::sdk::analytics::IMetadataTypes* neededMetadataTypes) override;

    virtual nx::sdk::Result<const nx::sdk::ISettingsResponse*> settingsReceived() override;

private:
    const std::string kEventType = "nx.gpt4vision.response";

    std::optional<open_ai::Response> sendVideoFrameToOpenAi(const open_ai::QueryPayload& payload);

    void showErrorMessage(const std::string& message);

private:
    const Engine* const m_engine;

    /** Last time when a query was sent. */
    std::chrono::microseconds m_lastQueryTimestamp{0us};

    bool m_queryInProgress = false;

    std::string m_queryText;

    std::chrono::seconds m_queryPeriod = kDefaultQueryPeriod;
};

} // namespace nx::vms_server_plugins::analytics::gpt4vision
