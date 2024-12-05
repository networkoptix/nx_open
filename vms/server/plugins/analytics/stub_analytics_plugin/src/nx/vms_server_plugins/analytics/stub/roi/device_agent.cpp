// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <nx/kit/utils.h>
#include <nx/kit/json.h>
#include <nx/sdk/helpers/settings_response.h>

#include "device_agent_settings_model.h"
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
    ConsumingDeviceAgent(deviceInfo, NX_DEBUG_ENABLE_OUTPUT, engine->integration()->instanceId()),
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

void DeviceAgent::doSetSettings(
    nx::sdk::Result<const nx::sdk::ISettingsResponse*>* /*outResult*/,
    const nx::sdk::IStringMap* settings)
{
    const char* testPolygon = settings->value(kTestPolygonSettingName.c_str());

    if (testPolygon)
        m_integrationSideTestPolygonSettingValue = testPolygon;
}

void DeviceAgent::getIntegrationSideSettings(
    Result<const ISettingsResponse*>* outResult) const
{
    // If the test polygon setting was set previously, return it in the response.
    if (!m_integrationSideTestPolygonSettingValue.empty())
    {
        const auto response = new SettingsResponse();
        response->setValue(kTestPolygonSettingName, m_integrationSideTestPolygonSettingValue);
        *outResult = response;
    }
}

} // namespace roi
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
