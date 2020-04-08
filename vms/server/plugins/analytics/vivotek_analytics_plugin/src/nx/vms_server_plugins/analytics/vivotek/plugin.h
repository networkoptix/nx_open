#pragma once

#include <nx/sdk/analytics/helpers/plugin.h>
#include <nx/sdk/analytics/i_engine.h>

namespace nx::vms_server_plugins::analytics::vivotek {

class Plugin: public nx::sdk::analytics::Plugin
{
protected:
    virtual nx::sdk::Result<nx::sdk::analytics::IEngine*> doObtainEngine() override;
    virtual std::string manifestString() const override;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
