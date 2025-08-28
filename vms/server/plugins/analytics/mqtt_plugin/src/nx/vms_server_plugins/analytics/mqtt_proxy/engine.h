// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/helpers/engine.h>
#include <pybind11/embed.h>
#include <pybind11/stl.h>

#include "python_app_host.h"

namespace nx::vms_server_plugins::analytics::mqtt_proxy {

class Engine: public nx::sdk::analytics::Engine
{
public:
    Engine(PythonAppHost& pythonAppHost);
    virtual ~Engine() override;

protected:
    virtual std::string manifestString() const override;
    virtual nx::sdk::Result<const nx::sdk::ISettingsResponse*> settingsReceived() override;
    virtual void doObtainDeviceAgent(
        nx::sdk::Result<nx::sdk::analytics::IDeviceAgent*>* outResult,
        const nx::sdk::IDeviceInfo* deviceInfo) override;

    virtual nx::sdk::Result<sdk::analytics::IAction::Result> executeAction(
        const std::string& actionId,
        nx::sdk::Uuid trackId,
        nx::sdk::Uuid deviceId,
        int64_t timestampUs,
        nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackInfo> objectTrackInfo,
        const std::map<std::string, std::string>& params) override;

private:
    void applySettingsToPythonApp(const std::map<std::string, std::string>& values);

private:
    PythonAppHost& m_pythonAppHost;
};

} // nx::vms_server_plugins::analytics::mqtt_proxy
