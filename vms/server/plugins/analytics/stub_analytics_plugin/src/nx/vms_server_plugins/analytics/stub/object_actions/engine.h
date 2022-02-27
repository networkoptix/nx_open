// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/helpers/engine.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_actions {

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

    virtual nx::sdk::Result<sdk::analytics::IAction::Result> executeAction(
        const std::string& actionId,
        nx::sdk::Uuid trackId,
        nx::sdk::Uuid deviceId,
        int64_t timestampUs,
        nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackInfo> objectTrackInfo,
        const std::map<std::string, std::string>& params) override;

private:
    nx::sdk::Result<sdk::analytics::IAction::Result> executeActionWithMessageResult(
        nx::sdk::Uuid trackId,
        nx::sdk::Uuid deviceId,
        int64_t timestampUs);

    nx::sdk::Result<sdk::analytics::IAction::Result> executeActionWithUrlResult();

    nx::sdk::Result<sdk::analytics::IAction::Result> executeActionWithParameters(
        nx::sdk::Uuid trackId,
        nx::sdk::Uuid deviceId,
        int64_t timestampUs,
        const std::map<std::string, std::string>& params);

    nx::sdk::Result<sdk::analytics::IAction::Result> executeActionWithRequirements(
        nx::sdk::Uuid trackId,
        nx::sdk::Uuid deviceId,
        int64_t timestampUs,
        nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackInfo> objectTrackInfo);
};

} // namespace object_actions
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
