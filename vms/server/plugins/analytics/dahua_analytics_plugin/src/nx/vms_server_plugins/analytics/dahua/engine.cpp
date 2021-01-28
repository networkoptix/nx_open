#include "engine.h"

#include <nx/utils/log/assert.h>
#include <nx/vms_server_plugins/utils/exception.h>
#include <nx/sdk/helpers/string.h>
#include <nx/fusion/serialization/json.h>

#include <QtCore/QJsonObject>

#include "plugin.h"
#include "device_agent.h"

namespace nx::vms_server_plugins::analytics::dahua {

using namespace nx::vms_server_plugins::utils;
using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine(Plugin* plugin):
    nx::sdk::analytics::Engine(false),
    m_plugin(plugin)
{
}


Plugin* Engine::plugin() const
{
    return m_plugin;
}

bool Engine::isCompatible(const IDeviceInfo* deviceInfo) const
{
    const auto vendor = QString(deviceInfo->vendor()).toLower();
    return vendor.startsWith("dahua");
}

std::string Engine::manifestString() const
{
    NX_ASSERT(false, "should never be called");
    return "";
}

void Engine::getManifest(Result<const IString*>* outResult) const
{
    interceptExceptions(outResult,
        [&]()
        {
            const auto manifest = QJsonObject{
                {"capabilities", "deviceDependent|noAutoBestShots"},
            };

            return new sdk::String(QJson::serialize(manifest).toStdString());
        });
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    interceptExceptions(outResult,
        [&]()
        {
            return new DeviceAgent(this, addRefToPtr(deviceInfo));
        });
}

} // namespace nx::vms_server_plugins::analytics::dahua
