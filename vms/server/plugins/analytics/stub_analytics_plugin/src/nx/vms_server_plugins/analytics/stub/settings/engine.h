// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/helpers/plugin.h>
#include <nx/sdk/analytics/helpers/engine.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace settings {

static const std::string kEnginePluginSideSetting = "testPluginSideSpinBox";
static const std::string kEnginePluginSideSettingValue = "42";

class Engine: public nx::sdk::analytics::Engine
{
public:
    Engine(nx::sdk::analytics::Plugin* plugin);
    virtual ~Engine() override;

    nx::sdk::analytics::Plugin* const plugin() const { return m_plugin; }

protected:
    virtual std::string manifestString() const override;

    virtual nx::sdk::Result<const nx::sdk::ISettingsResponse*> settingsReceived() override;

protected:
    virtual void doObtainDeviceAgent(
        nx::sdk::Result<nx::sdk::analytics::IDeviceAgent*>* outResult,
        const nx::sdk::IDeviceInfo* deviceInfo) override;

    virtual void getPluginSideSettings(
        nx::sdk::Result<const nx::sdk::ISettingsResponse*>* outResult) const override;

private:
    nx::sdk::analytics::Plugin* const m_plugin;
};

} // namespace settings
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
