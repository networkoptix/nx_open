// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <thread>

#include <nx/sdk/analytics/helpers/integration.h>
#include <nx/sdk/analytics/i_engine.h>

namespace nx::vms_server_plugins::analytics::mqtt_proxy {

class Integration: public nx::sdk::analytics::Integration
{
public:
    virtual ~Integration() override = default;

protected:
    virtual nx::sdk::Result<nx::sdk::analytics::IEngine*> doObtainEngine() override;
    virtual std::string manifestString() const override;
};

} // namespace nx::vms_server_plugins::analytics::mqtt_proxy
