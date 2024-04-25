// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <nx/kit/utils.h>
#include <nx/kit/json.h>
#include <nx/sdk/helpers/settings_response.h>

#include "stub_analytics_plugin_roi_ini.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace roi {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

DeviceAgent::DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo):
    ConsumingDeviceAgent(deviceInfo, NX_DEBUG_ENABLE_OUTPUT, engine->plugin()->instanceId()),
    m_engine(engine)
{
}

DeviceAgent::~DeviceAgent()
{
}

std::string DeviceAgent::manifestString() const
{
    return /*suppress newline*/ 1 + (const char*) R"json(
{
    "capabilities": "disableStreamSelection"
}
)json";
}

void DeviceAgent::doSetNeededMetadataTypes(
    Result<void>* /*outResult*/, const IMetadataTypes* neededMetadataTypes)
{
}

void DeviceAgent::getPluginSideSettings(
    Result<const ISettingsResponse*>* outResult) const
{
    const auto response = new SettingsResponse();

    nx::kit::Json::array jsonPoints{
        nx::kit::Json::array{0.138, 0.551},
        nx::kit::Json::array{0.775, 0.429},
        nx::kit::Json::array{0.748, 0.844}};

    nx::kit::Json::object jsonFigure;
    jsonFigure.insert(std::make_pair("points", jsonPoints));

    nx::kit::Json::object jsonResult;
    jsonResult.insert(std::make_pair("figure", jsonFigure));

    response->setValue("testPolygon", nx::kit::Json(jsonResult).dump());
    *outResult = response;
}

} // namespace roi
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
