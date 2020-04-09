#include "device_agent.h"
#include "nx/sdk/ptr.h"

#include <string>

#define NX_PRINT_PREFIX (m_logUtils.printPrefix)
#include <nx/kit/debug.h>
#include <nx/kit/json.h>
#include <nx/sdk/helpers/string.h>

#include "ini.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::kit;
using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace {

const std::string kNewTrackEventType = "nx.vivotek.newTrack";
const std::string kHelloWorldObjectType = "nx.vivotek.helloWorld";

std::string makePrintPrefix(const IDeviceInfo* deviceInfo)
{
    return "[" + libContext().name() + "_device" +
        (!deviceInfo ? "" : (std::string("_") + deviceInfo->id())) + "] ";
}

} // namespace

DeviceAgent::DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo):
    m_logUtils(NX_DEBUG_ENABLE_OUTPUT, makePrintPrefix(deviceInfo))
{
    deviceInfo->addRef();
    m_deviceInfo.reset(deviceInfo);
}

void DeviceAgent::doSetSettings(Result<const IStringMap*>* /*outResult*/, const IStringMap* settings)
{
    NX_OUTPUT << __func__;
    for (int i = 0; i < settings->count(); ++i)
        NX_OUTPUT << "    " << settings->key(i) << ": " << settings->value(i);
}

void DeviceAgent::getPluginSideSettings(Result<const ISettingsResponse*>* /*outResult*/) const
{
    NX_OUTPUT << __func__;
}

void DeviceAgent::getManifest(Result<const IString*>* outResult) const
{
    NX_OUTPUT << __func__;

    *outResult = new String(Json(Json::object{
        {"eventTypes", Json::array{
            Json::object{
                {"id", kNewTrackEventType},
                {"name", "New track started"},
            },
        }},
        {"objectTypes", Json::array{
            Json::object{
                {"id", kHelloWorldObjectType},
                {"name", "Hello, World!"},
            },
        }},
    }).dump());

    NX_OUTPUT << "    " << outResult->value()->str();
}

void DeviceAgent::setHandler(IHandler* handler)
{
    NX_OUTPUT << __func__;

    handler->addRef();
    m_handler.reset(handler);
}

void DeviceAgent::doSetNeededMetadataTypes(Result<void>* /*outResult*/, const IMetadataTypes* neededMetadataTypes)
{
    NX_OUTPUT << __func__;
    NX_OUTPUT << "    eventTypeIds:";
    const auto eventTypeIds = neededMetadataTypes->eventTypeIds();
    for (int i = 0; i < eventTypeIds->count(); ++i)
        NX_OUTPUT << "        " << eventTypeIds->at(i);
    NX_OUTPUT << "    objectTypeIds:";
    const auto objectTypeIds = neededMetadataTypes->objectTypeIds();
    for (int i = 0; i < objectTypeIds->count(); ++i)
        NX_OUTPUT << "        " << objectTypeIds->at(i);
}

} // namespace nx::vms_server_plugins::analytics::vivotek
