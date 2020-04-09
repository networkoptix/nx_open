#include "engine.h"

#include <string_view>

#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>
#include <nx/kit/json.h>

#include "ini.h"
#include "device_agent.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace std::literals;

using namespace nx::kit;
using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine():
    nx::sdk::analytics::Engine(NX_DEBUG_ENABLE_OUTPUT)
{
}

bool Engine::isCompatible(const IDeviceInfo* deviceInfo) const
{
    return deviceInfo->vendor() == "VIVOTEK"sv;
}

std::string Engine::manifestString() const
{
    return Json(Json::object{}).dump();
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    *outResult = new DeviceAgent(deviceInfo);
}

} // namespace nx::vms_server_plugins::analytics::vivotek
