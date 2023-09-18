// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/helpers/engine.h>
#include <nx/sdk/analytics/helpers/plugin.h>
#include <nx/sdk/analytics/i_compressed_video_packet.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace sample {

class Engine: public nx::sdk::analytics::Engine
{
public:
    Engine();
    virtual ~Engine() override;

protected:
    virtual std::string manifestString() const override;

protected:
    virtual void doObtainDeviceAgent(
        nx::sdk::Result<nx::sdk::analytics::IDeviceAgent*>* outResult,
        const nx::sdk::IDeviceInfo* deviceInfo) override;
};

} // namespace sample
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
