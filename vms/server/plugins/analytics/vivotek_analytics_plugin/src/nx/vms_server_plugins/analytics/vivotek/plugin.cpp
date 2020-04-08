#include "plugin.h"

#include "engine.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Result<IEngine*> Plugin::doObtainEngine()
{
    return new Engine();
}

std::string Plugin::manifestString() const
{
    return /*suppress newline*/ 1 + (const char*) R"json(
{
    "id": "nx.vivotek",
    "name": "Vivotek analytics plugin",
    "description": "Supports analytics on Vivotek cameras.",
    "version": "0.0.0",
    "vendor": "Network Optix"
}
)json";
}

extern "C" NX_PLUGIN_API nx::sdk::IPlugin* createNxPlugin()
{
    return new Plugin();
}

} // namespace nx::vms_server_plugins::analytics::vivotek
