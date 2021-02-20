#include "plugin.h"

#include <nx/vms_server_plugins/utils/exception.h>
#include <nx/fusion/serialization/json.h>
#include <nx/sdk/helpers/string.h>

#include <QtCore/QJsonObject>

#include "engine.h"

namespace nx::vms_server_plugins::analytics::dahua {

using namespace nx::vms_server_plugins::utils;
using namespace nx::sdk;
using namespace nx::sdk::analytics;

std::chrono::milliseconds Plugin::vmsSystemTimeSinceEpoch() const
{
    return std::chrono::milliseconds(utilityProvider()->vmsSystemTimeSinceEpochMs());
}

void Plugin::getManifest(Result<const IString*>* outResult) const
{
    interceptExceptions(outResult,
        [&]()
        {
            const auto manifest = QJsonObject{
                {"id", "nx.dahua"},
                {"name", "Dahua Camera Analytics"},
                {"description", "Enables in-camera analytics support for Dahua IP Cameras"},
                {"version", "1.0.0"},
                {"vendor", "Dahua"},
            };

            return new sdk::String(QJson::serialize(manifest).toStdString());
        });
}

void Plugin::doCreateEngine(Result<IEngine*>* outResult)
{
    interceptExceptions(outResult,
        [&]()
        {
            return new Engine(this);
        });
}

extern "C" NX_PLUGIN_API nx::sdk::IPlugin* createNxPlugin()
{
    return new Plugin();
}

} // namespace nx::vms_server_plugins::analytics::dahua
