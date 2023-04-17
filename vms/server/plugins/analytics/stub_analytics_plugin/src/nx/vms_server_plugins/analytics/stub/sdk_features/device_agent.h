// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <nx/sdk/analytics/helpers/consuming_device_agent.h>

#include "engine.h"
#include "stub_analytics_plugin_sdk_features_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace sdk_features {

class DeviceAgent: public nx::sdk::analytics::ConsumingDeviceAgent
{
public:
    DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo);
    virtual ~DeviceAgent() override;

protected:
    virtual void doSetNeededMetadataTypes(
        nx::sdk::Result<void>* outValue,
        const nx::sdk::analytics::IMetadataTypes* neededMetadataTypes) override;

    virtual std::string manifestString() const override;

private:
    Engine* const m_engine;
};

} // namespace sdk_features
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
