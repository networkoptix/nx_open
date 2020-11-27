#pragma once

#include <nx/sdk/analytics/helpers/plugin.h>

namespace nx::vms_server_plugins::analytics::dahua {

class Plugin:
    public nx::sdk::analytics::Plugin
{
protected:
    virtual void getManifest(nx::sdk::Result<const nx::sdk::IString*>* outResult) const override;
    virtual void doCreateEngine(nx::sdk::Result<nx::sdk::analytics::IEngine*>* outResult) override;
};

} // namespace nx::vms_server_plugins::analytics::dahua
