#include "engine.h"

#include "device_agent.h"

#include <nx/kit/json.h>

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::kit;
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
    return Json(Json::object{}).dump();
}

} // namespace nx::vms_server_plugins::analytics::vivotek
