// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include <nx/sdk/i_device_info.h>
#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/error.h>
#include <nx/sdk/helpers/settings_response.h>

#include "../utils.h"

#include "device_agent.h"
#include "stub_analytics_plugin_custom_metadata_ini.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace custom_metadata {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine(Plugin* plugin):
    nx::sdk::analytics::Engine(NX_DEBUG_ENABLE_OUTPUT, plugin->instanceId()),
    m_plugin(plugin)
{
}

Engine::~Engine()
{
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    *outResult = new DeviceAgent(this, deviceInfo);
}

static std::string buildCapabilities()
{
    if (ini().deviceDependent)
        return "deviceDependent";

    return "";
}

static std::string buildStreamTypeFilter()
{
    if (ini().needMetadata)
        return "metadata";

    return "";
}

std::string Engine::manifestString() const
{
    std::string result = /*suppress newline*/ 1 + (const char*)
R"json(
{
    "capabilities": ")json" + buildCapabilities() + R"json(",
    "streamTypeFilter": ")json" + buildStreamTypeFilter() + R"json("
}
)json";

    return result;
}

} // namespace custom_metadata
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
