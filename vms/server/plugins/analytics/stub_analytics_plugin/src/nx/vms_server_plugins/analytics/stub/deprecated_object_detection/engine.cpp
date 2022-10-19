// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

#include <nx/sdk/i_device_info.h>
#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/error.h>
#include <nx/sdk/helpers/settings_response.h>

#include "../utils.h"
#include "objects/vehicles.h"
#include "objects/human_face.h"
#include "objects/stone.h"

#include "device_agent.h"
#include "settings_model.h"
#include "stub_analytics_plugin_deprecated_object_detection_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace deprecated_object_detection {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

using namespace std::chrono;
using namespace std::literals::chrono_literals;

Engine::Engine(Plugin* plugin):
    nx::sdk::analytics::Engine(NX_DEBUG_ENABLE_OUTPUT, plugin->instanceId()),
    m_plugin(plugin)
{
}

Engine::~Engine()
{
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    *outResult = new DeviceAgent(this, deviceInfo);
}

static std::string buildCapabilities()
{
    std::string capabilities;

    if (ini().deviceDependent)
        capabilities += "|deviceDependent";

    if (ini().keepObjectBoundingBoxRotation)
        capabilities += "|keepObjectBoundingBoxRotation";

    // Delete first '|', if any.
    if (!capabilities.empty() && capabilities.at(0) == '|')
        capabilities.erase(0, 1);

    return capabilities;
}

std::string Engine::manifestString() const
{
    std::string result = /*suppress newline*/ 1 + (const char*) R"json(
{
    "typeLibrary":
    {
        "enumTypes":
        [
            {
                "id": "nx.stub.CarBodyType",
                "name": "Car body type",
                "items": ["sedan", "hatchback", "pickup", "suv", "minivan"]
            },
            {
                "id": "nx.stub.ExtendedCarBodyType",
                "name": "Extended car body type",
                "base": "nx.stub.CarBodyType",
                "items": ["coupe"]
            }
        ],
        "objectTypes":
        [)json";

    if (ini().declareStubObjectTypes)
    {
        result += R"json(
        {
            "id": ")json" + kVehicleObjectType + R"json(",
                "name": "Vehicle",
                "icon": "vehicle.svg",
                "attribute": [
                    {
                        "name": "Color",
                        "type": "Color"
                    },
                    {
                        "name": "Passenger count",
                        "type": "Number",
                        "subtype": "integer",
                        "minValue": 0,
                        "maxValue": 100
                    },
                    {
                        "name": "Specific base type attribute",
                        "type": "Number"
                    }
                ]
            },
            {
                "id": ")json" + kCarObjectType + R"json(",
                "name": "Car",
                "icon": "car.svg",
                "base": ")json" + kVehicleObjectType + R"json(",
                "omittedBaseAttributes": ["Specific base type attribute"],
                "attributes": [
                    {
                        "name": "Weight",
                        "type": "Number",
                        "minValue": 0,
                        "unit": "kg"
                    },
                    {
                        "name": "Is self driving",
                        "type": "Boolean"
                    },
                    {
                        "name": "Registration plate",
                        "type": "String"
                    },
                    {
                        "name": "Car body type",
                        "type": "Enum",
                        "subtype": "nx.stub.ExtendedCarBodyType"
                    }
                ]
            },
            {
                "id": "nx.stub.glasses",
                "name": "Glasses",
                "attributes": [
                    {
                        "name": "Color",
                        "type": "String"
                    },
                    {
                        "name": "Shape",
                        "type": "String"
                    }
                ]
            },
            {
                "id": ")json" + kHumanFaceObjectType + R"json(",
                "name": "Human face",
                "icon": "human-face.svg",
                "attributes": [
                    {
                        "name": "Glasses",
                        "type": "Object",
                        "subtype": "nx.stub.glasses"
                    }
                ]
            },
            {
                "id": ")json" + kStoneObjectType + R"json(",
                "name": "Stone"
            })json";
    }

    result += R"json(
        ]
    },
    "capabilities": ")json" + buildCapabilities() + R"json(",
    "streamTypeFilter": "compressedVideo",
    "deviceAgentSettingsModel":
)json"
        + kSettingsModelPart1
        + (ini().declareStubObjectTypes ? kStubObjectTypesSettings : "")
        + kSettingsModelPart2
        + R"json(
}
)json";

    return result;
}

} // namespace deprecated_object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
