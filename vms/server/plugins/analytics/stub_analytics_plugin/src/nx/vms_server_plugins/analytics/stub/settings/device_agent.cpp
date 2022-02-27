// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>
#include <nx/kit/utils.h>

#include <nx/sdk/helpers/settings_response.h>

#include "settings_model.h"
#include "stub_analytics_plugin_settings_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace settings {

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

Result<const ISettingsResponse*> DeviceAgent::settingsReceived()
{
    const auto settingsResponse = new sdk::SettingsResponse();

    const std::string settingsModel = settingValue(kSettingsModelSettings);

    if (ini().sendSettingsModelWithValues)
    {
        if (settingsModel == kAlternativeSettingsModelOption)
        {
            settingsResponse->setModel(kAlternativeSettingsModel);
        }
        else if (settingsModel == kRegularSettingsModelOption)
        {
            const std::string languagePart = (settingValue(kCitySelector) == kGermanOption)
                ? kGermanCitiesSettingsModelPart
                : kEnglishCitiesSettingsModelPart;

            settingsResponse->setModel(kRegularSettingsModelPart1
                + languagePart
                + kRegularSettingsModelPart2);
        }
    }

    // The manifest depends on some of the above parsed settings, so sending the new manifest.
    pushManifest(manifestString());

    return settingsResponse;
}

void DeviceAgent::doSetNeededMetadataTypes(
    Result<void>* /*outResult*/, const IMetadataTypes* /*neededMetadataTypes*/)
{
}

void DeviceAgent::getPluginSideSettings(
    Result<const ISettingsResponse*>* outResult) const
{
    const auto response = new SettingsResponse();

    response->setValue("pluginSideTestSpinBox", "100");

    *outResult = response;
}

} // namespace settings
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
