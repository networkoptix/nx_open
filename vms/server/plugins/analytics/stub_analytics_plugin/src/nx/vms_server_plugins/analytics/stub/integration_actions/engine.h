// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/helpers/engine.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace integration_actions {

const std::string kIntegrationActionId = "nx.stub.integrationAction";

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

    virtual nx::sdk::Result<sdk::analytics::IAction::Result> executeIntegrationAction(
        const std::string& actionId,
        int64_t timestampUs,
        const std::map<std::string, std::string>& params,
        const std::string& state) override;
};

} // namespace integration_actions
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
