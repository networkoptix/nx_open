// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/helpers/plugin.h>
#include <nx/sdk/analytics/i_engine.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

class Plugin: public nx::sdk::analytics::Plugin
{
public:
    Plugin() = default;

    Plugin(int instanceIndexForMultiPluginMode):
        m_instanceIndexForMultiPluginMode(instanceIndexForMultiPluginMode)
    {
    }

protected:
    virtual nx::sdk::Result<nx::sdk::analytics::IEngine*> doObtainEngine() override;
    virtual std::string manifestString() const override;

private:
    int m_instanceIndexForMultiPluginMode = -1;
};

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
