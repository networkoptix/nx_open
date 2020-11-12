#include "plugin.h"

#include <nx/vms_server_plugins/utils/exception.h>
#include <nx/sdk/helpers/string.h>

#include <QtCore/QJsonObject>

#include "engine.h"
#include "json_utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::vms_server_plugins::utils;
using namespace nx::sdk;
using namespace nx::sdk::analytics;

void Plugin::getManifest(Result<const IString*>* outResult) const
{
    interceptExceptions(outResult,
        [&]()
        {
            const auto manifest = QJsonObject{
                {"id", "nx.vivotek"},
                {"name", "VIVOTEK analytics plugin"},
                {"description", "Supports analytics on VIVOTEK cameras."},
                {"version", "1.0.0"},
                {"vendor", "VIVOTEK"},
            };

            return new String(serializeJson(manifest).toStdString());
        });
}

void Plugin::doCreateEngine(Result<IEngine*>* outResult)
{
    interceptExceptions(outResult,
        [&]()
        {
            return new Engine(*this);
        });
}

extern "C" NX_PLUGIN_API nx::sdk::IPlugin* createNxPlugin()
{
    return new Plugin();
}

} // namespace nx::vms_server_plugins::analytics::vivotek
