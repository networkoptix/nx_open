#include "engine.h"

#include <string_view>
#include <exception>
#include <utility>

#include <nx/utils/log/assert.h>
#include <nx/vms_server_plugins/utils/exception.h>
#include <nx/kit/debug.h>

#include <QtCore/QJsonObject>

#include "plugin.h"
#include "ini.h"
#include "device_agent.h"
#include "json_utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace std::literals;
using namespace nx::vms_server_plugins::utils;
using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine(Plugin& plugin):
    nx::sdk::analytics::Engine(NX_DEBUG_ENABLE_OUTPUT),
    m_plugin(plugin)
{
}

Plugin& Engine::plugin() const
{
    return m_plugin;
}

bool Engine::isCompatible(const IDeviceInfo* deviceInfo) const
{
    return deviceInfo->vendor() == "VIVOTEK"sv;
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
                {"capabilities", "deviceDependent"},
            };

            return new sdk::String(serializeJson(manifest).toStdString());
        });
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    interceptExceptions(outResult,
        [&]()
        {
            return new DeviceAgent(*this, deviceInfo);
        });
}

} // namespace nx::vms_server_plugins::analytics::vivotek
