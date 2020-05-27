#include "plugin.h"

#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

#include <QtCore/QJsonObject>

#include "engine.h"
#include "json_utils.h"

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
    return unparseJson(QJsonObject{
        {"id", "nx.vivotek"},
        {"name", "Vivotek analytics plugin"},
        {"description", "Supports analytics on Vivotek cameras."},
        {"version", "0.0.0"},
        {"vendor", "Network Optix"},
    }).toStdString();
}

extern "C" NX_PLUGIN_API nx::sdk::IPlugin* createNxPlugin()
{
    return new Plugin();
}

} // namespace nx::vms_server_plugins::analytics::vivotek
