// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/sdk/uuid.h>
#include <nx/sdk/analytics/helpers/plugin.h>
#include <nx/sdk/analytics/helpers/engine.h>
#include <nx/sdk/analytics/i_uncompressed_video_frame.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace motion_metadata {

class Engine: public nx::sdk::analytics::Engine
{
public:
    Engine(nx::sdk::analytics::Plugin* plugin);
    virtual ~Engine() override;

    nx::sdk::analytics::Plugin* const plugin() const { return m_plugin; }

protected:
    virtual std::string manifestString() const override;

protected:
    virtual void doObtainDeviceAgent(
        nx::sdk::Result<nx::sdk::analytics::IDeviceAgent*>* outResult,
        const nx::sdk::IDeviceInfo* deviceInfo) override;

private:
    nx::sdk::analytics::Plugin* const m_plugin;
};

} // namespace motion_metadata
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
