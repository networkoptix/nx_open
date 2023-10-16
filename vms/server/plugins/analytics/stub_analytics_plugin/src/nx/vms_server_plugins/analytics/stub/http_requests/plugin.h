// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/helpers/plugin.h>
#include <nx/sdk/analytics/i_engine.h>

namespace nx::vms_server_plugins::analytics::stub::http_requests {

class Plugin: public nx::sdk::analytics::Plugin
{
public:
    nx::sdk::Ptr<nx::sdk::IUtilityProvider> utilityProvider() const;

protected:
    virtual nx::sdk::Result<nx::sdk::analytics::IEngine*> doObtainEngine() override;
    virtual std::string manifestString() const override;
};

} // namespace nx::vms_server_plugins::analytics::stub::http_requests
