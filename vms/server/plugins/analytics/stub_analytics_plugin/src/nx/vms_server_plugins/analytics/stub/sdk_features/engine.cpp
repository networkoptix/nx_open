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

Engine::Engine(Integration* integration):
    nx::sdk::analytics::Engine(NX_DEBUG_ENABLE_OUTPUT, integration->instanceId()),
    m_integration(integration)
{
    obtainServerSdkVersion();
    obtainPluginHomeDir();
    printServerDataDir();
    readDataFromSharedContextStorage();
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

void Engine::printServerDataDir()
{
    const auto utilityProvider = m_integration->utilityProvider();
    if (!NX_KIT_ASSERT(utilityProvider))
        return;

    std::string dataDir = utilityProvider->dataDir();

    if (dataDir.empty())
        NX_PRINT << "Server data dir: absent";
    else
        NX_PRINT << "Server data dir: " << nx::kit::utils::toString(dataDir);
}

// This function demonstrates how to read data from Shared Context Storage. By default, the Server
// does not have any data in the Shared Context Storage, so the function will print "absent"
// messages.
void Engine::readDataFromSharedContextStorage()
{
    const auto utilityProvider = m_integration->utilityProvider();
    if (!NX_KIT_ASSERT(utilityProvider))
        return;

    auto value1 = utilityProvider->sharedContextValue("id", "key1");
    if (value1)
        NX_PRINT << "Shared context storage value (id:key1): "
            << nx::kit::utils::toString(*value1);
    else
        NX_PRINT << "Shared context storage value (id:key1): absent";

    auto value2 = utilityProvider->sharedContextValue("id", "key2");
    if (value2)
    {
        NX_PRINT << "Shared context storage value (id:key2) as string: "
            << nx::kit::utils::toString(*value2);
    }
    else
        NX_PRINT << "Shared context storage value (id:key2): absent";
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
