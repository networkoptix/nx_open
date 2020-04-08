#include "engine.h"

#include "device_agent.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine():
    nx::sdk::analytics::Engine(/*enableOutput*/ true)
{
}

Engine::~Engine()
{
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    *outResult = new DeviceAgent(deviceInfo);
}

std::string Engine::manifestString() const
{
    return /*suppress newline*/ 1 + (const char*) R"json(
{
}
)json";
}

} // namespace nx::vms_server_plugins::analytics::vivotek
