// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"
#include "device_agent.h"
#include "device_agent_manifest.h"

#include <nx/kit/json.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_detection {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

static const std::set<std::string> kObjectTypeIdsGeneratedByDefault = {
    "nx.base.Bike",
    "nx.base.Bus",
    "nx.base.LicensePlate",
    "nx.base.Face",
    "nx.base.Person"
};

Engine::Engine(): nx::sdk::analytics::Engine(/*enableOutput*/ true)
{
}

Engine::~Engine()
{
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    *outResult = new DeviceAgent(deviceInfo);
}

std::string Engine::manifestString() const
{
    using namespace nx::kit;

    std::string errors;
    Json deviceAgentManifest = Json::parse(kDeviceAgentManifest, errors).object_items();

    Json::array generationSettings;
    for (const auto& supportedType : deviceAgentManifest["supportedTypes"].array_items())
    {
        Json::object supportedTypeObject = supportedType.object_items();
        const std::string& objectTypeId = supportedTypeObject["objectTypeId"].string_value();
        Json::object generationSetting = {
            {"type", "CheckBox"},
            {"name", DeviceAgent::kObjectTypeGenerationSettingPrefix + objectTypeId},
            {"caption", objectTypeId},
            {
                "defaultValue",
                kObjectTypeIdsGeneratedByDefault.find(objectTypeId)
                    != kObjectTypeIdsGeneratedByDefault.cend()
            }
        };

        generationSettings.push_back(std::move(generationSetting));
    }

    Json::object settingsModel = {
        {"type", "Settings"},
        {"items", generationSettings}
    };

    Json::object engineManifest = {
        {"streamTypeFilter", "compressedVideo"},
        {"deviceAgentSettingsModel", settingsModel}
    };

    return Json(engineManifest).dump();
}

} // namespace object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
