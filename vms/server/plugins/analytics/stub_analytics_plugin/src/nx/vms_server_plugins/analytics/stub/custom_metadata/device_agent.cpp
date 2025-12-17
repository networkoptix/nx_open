// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <nx/kit/utils.h>

#include "stub_analytics_plugin_custom_metadata_ini.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

namespace nx::vms_server_plugins::analytics::stub::custom_metadata {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

DeviceAgent::DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo):
    ConsumingDeviceAgent(deviceInfo, NX_DEBUG_ENABLE_OUTPUT, engine->integration()->instanceId())
{
}

DeviceAgent::~DeviceAgent()
{
}

std::string DeviceAgent::manifestString() const
{
    std::string result = /*suppress newline*/ 1 + (const char*)R"json(
{
}
)json";

    return result;
}

bool DeviceAgent::pushCustomMetadataPacket(
    Ptr<const nx::sdk::analytics::ICustomMetadataPacket> customMetadataPacket)
{
    if (!ini().needMetadata)
    {
        NX_PRINT <<
            "ERROR: Received Custom Metadata packet, contrary to streamTypeFilter in Manifest.";
        return false;
    }

    NX_PRINT << nx::kit::utils::format(
        "Received Custom Metadata packet: %d bytes, timestamp %d us.",
        customMetadataPacket->dataSize(), customMetadataPacket->timestampUs());

    return true;
}

} // namespace nx::vms_server_plugins::analytics::stub::custom_metadata
