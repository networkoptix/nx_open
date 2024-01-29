// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/helpers/engine.h>
#include <nx/sdk/analytics/helpers/plugin.h>
#include <nx/sdk/analytics/i_compressed_video_packet.h>

namespace nx::vms_server_plugins::analytics::gpt4vision {

class Engine: public nx::sdk::analytics::Engine
{
public:
    Engine();
    virtual ~Engine() override;

public:
    static constexpr std::string_view kApiKeyParameter = "apiKey";

    std::string apiKey() const { return m_apiKey; };

protected:
    virtual std::string manifestString() const override;
    virtual nx::sdk::Result<const nx::sdk::ISettingsResponse*> settingsReceived() override;

protected:
    virtual void doObtainDeviceAgent(
        nx::sdk::Result<nx::sdk::analytics::IDeviceAgent*>* outResult,
        const nx::sdk::IDeviceInfo* deviceInfo) override;

private:
    std::string m_apiKey;
};

} // namespace nx::vms_server_plugins::analytics::gpt4vision
