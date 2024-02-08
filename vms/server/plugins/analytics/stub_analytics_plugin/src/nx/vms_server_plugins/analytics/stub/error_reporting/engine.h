// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/helpers/engine.h>
#include <nx/sdk/analytics/helpers/plugin.h>
#include <nx/sdk/uuid.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace error_reporting {

class Engine: public nx::sdk::analytics::Engine
{
public:
    Engine(nx::sdk::analytics::Plugin* plugin);
    virtual ~Engine() override;

    nx::sdk::analytics::Plugin* const plugin() const { return m_plugin; }

protected:
    virtual std::string manifestString() const override;

    virtual void doObtainDeviceAgent(
        nx::sdk::Result<nx::sdk::analytics::IDeviceAgent*>* outResult,
        const nx::sdk::IDeviceInfo* deviceInfo) override;

    virtual void doSetSettings(
        nx::sdk::Result<const nx::sdk::ISettingsResponse*>* outResult,
        const nx::sdk::IStringMap* settings) override;

    virtual void getPluginSideSettings(
        nx::sdk::Result<const nx::sdk::ISettingsResponse*>* outResult) const override;

    virtual void doGetSettingsOnActiveSettingChange(
        nx::sdk::Result<const nx::sdk::IActiveSettingChangedResponse*>* outResult,
        const nx::sdk::IActiveSettingChangedAction* activeSettingChangedAction) override;

    virtual nx::sdk::Result<const nx::sdk::ISettingsResponse*> settingsReceived() override;

private:
    nx::sdk::analytics::Plugin* const m_plugin;
};

} // namespace error_reporting
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
