// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include "device_agent.h"
#include "stub_analytics_plugin_sdk_features_ini.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace sdk_features {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

using namespace std::chrono;
using namespace std::literals::chrono_literals;

Engine::Engine(Integration* integration):
    nx::sdk::analytics::Engine(NX_DEBUG_ENABLE_OUTPUT, integration->instanceId()),
    m_integration(integration)
{
    obtainServerSdkVersion();
    obtainPluginHomeDir();
}

Engine::~Engine()
{
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    *outResult = new DeviceAgent(this, deviceInfo);
}

void Engine::obtainServerSdkVersion()
{
    const auto utilityProvider = m_integration->utilityProvider();
    if (!NX_KIT_ASSERT(utilityProvider))
        return;

    auto serverSdkVersion = utilityProvider->serverSdkVersion();
    NX_PRINT << "SDK version built into the Server: " << nx::kit::utils::toString(serverSdkVersion);
}

void Engine::obtainPluginHomeDir()
{
    const auto utilityProvider = m_integration->utilityProvider();
    if (!NX_KIT_ASSERT(utilityProvider))
        return;

    m_integrationHomeDir = utilityProvider->homeDir();

    if (m_integrationHomeDir.empty())
        NX_PRINT << "Plugin home dir: absent";
    else
        NX_PRINT << "Plugin home dir: " << nx::kit::utils::toString(m_integrationHomeDir);
}

static std::string buildCapabilities()
{
    if (ini().deviceDependent)
        return "deviceDependent";

    return "";
}

std::string Engine::manifestString() const
{
    std::string result = /*suppress newline*/ 1 + (const char*)
R"json(
{
    "capabilities": ")json" + buildCapabilities() + R"json("
}
)json";

    return result;
}

} // namespace sdk_features
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
