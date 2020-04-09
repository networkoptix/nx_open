#include "plugin.h"

#include "engine.h"

#include <nx/kit/json.h>

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::kit;
using namespace nx::sdk;
using namespace nx::sdk::analytics;

Result<IEngine*> Plugin::doObtainEngine()
{
    return new Engine();
}

std::string Plugin::manifestString() const
{
    return Json(Json::object{
        {"id", "nx.vivotek"},
        {"name", "Vivotek analytics plugin"},
        {"description", "Supports analytics on Vivotek cameras."},
        {"version", "0.0.0"},
        {"vendor", "Network Optix"},
    }).dump();
}

extern "C" NX_PLUGIN_API nx::sdk::IPlugin* createNxPlugin()
{
    return new Plugin();
}

} // namespace nx::vms_server_plugins::analytics::vivotek
