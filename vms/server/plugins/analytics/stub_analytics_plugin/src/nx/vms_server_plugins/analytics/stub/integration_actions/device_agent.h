// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/helpers/consuming_device_agent.h>

#include "engine.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace integration_actions {

class DeviceAgent: public nx::sdk::analytics::ConsumingDeviceAgent
{
public:
    DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo);
    virtual ~DeviceAgent() override;

protected:
    virtual std::string manifestString() const override;

    virtual void doSetNeededMetadataTypes(
        nx::sdk::Result<void>* outValue,
        const nx::sdk::analytics::IMetadataTypes* neededMetadataTypes) override;

    virtual void doPushDataPacket(
        nx::sdk::Result<void>* outResult,
        nx::sdk::analytics::IDataPacket* dataPacket) override;
};

} // namespace integration_actions
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
