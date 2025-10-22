// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace integration_actions {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

DeviceAgent::DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo):
    ConsumingDeviceAgent(deviceInfo, /*enableOutput*/ true)
{
}

DeviceAgent::~DeviceAgent()
{
}

std::string DeviceAgent::manifestString() const
{
    return /*suppress newline*/ 1 + (const char*) R"json(
{
    "capabilities": "disableStreamSelection"
}
)json";
}

void DeviceAgent::doSetNeededMetadataTypes(
    nx::sdk::Result<void>* /*outValue*/,
    const nx::sdk::analytics::IMetadataTypes* /*neededMetadataTypes*/)
{
}

void DeviceAgent::doPushDataPacket(
    nx::sdk::Result<void>* /*outResult*/,
    nx::sdk::analytics::IDataPacket* /*dataPacket*/)
{
}

} // namespace integration_actions
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
