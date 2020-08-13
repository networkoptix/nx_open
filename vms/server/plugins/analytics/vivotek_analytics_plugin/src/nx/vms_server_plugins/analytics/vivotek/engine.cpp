#include "engine.h"

#include <string_view>
#include <exception>
#include <utility>

#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>
#include <nx/sdk/helpers/error.h>

#include <QtCore/QJsonObject>

#include "ini.h"
#include "device_agent.h"
#include "exception.h"
#include "json_utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace std::literals;

using namespace nx::kit;
using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine(Ptr<IUtilityProvider> utilityProvider):
    nx::sdk::analytics::Engine(NX_DEBUG_ENABLE_OUTPUT),
    m_utilityProvider(std::move(utilityProvider))
{
}

bool Engine::isCompatible(const IDeviceInfo* deviceInfo) const
{
    return deviceInfo->vendor() == "VIVOTEK"sv;
}

std::string Engine::manifestString() const
{
    return serializeJson(QJsonObject{}).toStdString();
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    try
    {
        *outResult = new DeviceAgent(deviceInfo, m_utilityProvider);
    }
    catch (const Exception& exception)
    {
        *outResult = exception.toSdkError();
    }
    catch (const std::exception& exception)
    {
        *outResult = error(ErrorCode::internalError, exception.what());
    }
}

} // namespace nx::vms_server_plugins::analytics::vivotek
