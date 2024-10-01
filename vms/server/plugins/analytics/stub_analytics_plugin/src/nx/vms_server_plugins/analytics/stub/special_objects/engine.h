// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/sdk/analytics/helpers/engine.h>
#include <nx/sdk/analytics/helpers/integration.h>
#include <nx/sdk/analytics/i_uncompressed_video_frame.h>
#include <nx/sdk/uuid.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace special_objects {

class Engine: public nx::sdk::analytics::Engine
{
public:
    Engine(nx::sdk::analytics::Integration* integration);
    virtual ~Engine() override;

    nx::sdk::analytics::Integration* const integration() const { return m_integration; }

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
    nx::sdk::analytics::Integration* const m_integration;
};

} // namespace special_objects
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
