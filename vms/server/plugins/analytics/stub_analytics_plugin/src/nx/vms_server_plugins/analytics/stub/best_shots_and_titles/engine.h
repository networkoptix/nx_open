// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/helpers/engine.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace best_shots {

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

} // namespace best_shots
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
