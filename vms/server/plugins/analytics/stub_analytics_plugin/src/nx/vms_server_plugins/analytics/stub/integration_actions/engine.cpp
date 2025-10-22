// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include "device_agent.h"

#include <nx/kit/utils.h>
#include <nx/sdk/helpers/string.h>

#include "stub_analytics_plugin_integration_actions_ini.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace integration_actions {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

static std::string indent(int level)
{
    const int spaceCount = level * 4;
    return std::string(spaceCount, ' ');
}

static std::string parametersToString(const std::map<std::string, std::string>& parameters)
{
    std::string result;

    for (const auto& entry: parameters)
    {
        const std::string& parameterName = entry.first;
        const std::string& parameterValue = entry.second;

        result += parameterName + ":" + parameterValue + ", ";
    }

    if (!parameters.empty())
        result = result.substr(0, result.size() - 2);

    return result;
}

Engine::Engine(): nx::sdk::analytics::Engine(ini().enableOutput)
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
    return /*suppress newline*/ 1 + (const char*)
R"json(
{
    "integrationActions":
    [
        {
            "id": ")json" + kIntegrationActionId + R"json(",
            "name": "Stub: Integration Action",
            "description": "Print serialized parameters to debug output",
            "isInstant": true,
            "fields":
            [
                {
                    "type": "int",
                    "fieldName": "int",
                    "displayName": "Integer value",
                    "description": "An example of an integer field"
                },
                {
                    "type": "flag",
                    "fieldName": "flag",
                    "displayName": "Flag",
                    "description": "An example of a flag (boolean) field"
                },
                {
                    "type": "stringSelection",
                    "fieldName": "stringSelection",
                    "displayName": "Combo Box",
                    "description": "An example of a string selection field",
                    "properties":
                    {
                        "items":
                        [
                            {
                                "name": "Option 1",
                                "value": "option1"
                            },
                            {
                                "name": "Option 2",
                                "value": "option2"
                            }
                        ]
                    }
                },
                {
                    "type": "password",
                    "fieldName": "password",
                    "displayName": "Password",
                    "description": "An example of a password field"
                },
                {
                    "type": "devices",
                    "fieldName": "deviceIds",
                    "displayName": "Devices",
                    "description": "An example of device selection field",
                    "properties":
                    {
                        "acceptAll": true,
                        "useSource": true,
                        "validationPolicy": ""
                    }
                },
                {
                    "type": "layouts",
                    "fieldName": "layoutIds",
                    "displayName": "Layouts",
                    "description": "An example of layout selection field"
                },
                {
                    "type": "servers",
                    "fieldName": "serverIds",
                    "displayName": "Servers",
                    "description": "An example of server selection field",
                    "properties":
                    {
                        "acceptAll": true,
                        "allowEmptySelection": true,
                        "validationPolicy": "hasBuzzer"
                    }
                },
                {
                    "type": "users",
                    "fieldName": "userIds",
                    "displayName": "Users",
                    "description": "An example of user selection field",
                    "properties":
                    {
                        "acceptAll": false,
                        "allowEmptySelection": false,
                        "validationPolicy": ""
                    }
                },
                {
                    "type": "text",
                    "fieldName": "text",
                    "displayName": "Line",
                    "description": "An example of a simple line of text field"
                },
                {
                    "type": "textWithFields",
                    "fieldName": "textWithFields",
                    "displayName": "Text",
                    "description": "An example of a text with fields substitution and default value",
                    "properties":
                    {
                        "text": "{event.description}"
                    }
                },
                {
                    "type": "time",
                    "fieldName": "time",
                    "displayName": "Duration",
                    "description": "An example of a time duration field",
                    "properties":
                    {
                        "min": 0,
                        "max": 100
                    }
                }
            ]
        }
    ]
}
)json";
}

Result<IAction::Result> Engine::executeIntegrationAction(
    const std::string& actionId,
    int64_t timestampUs,
    const std::map<std::string, std::string>& params,
    const std::string& state)
{
    NX_PRINT << "Going to execute an Action. Action id: " << actionId
        << ", state: " << state
        << ", timestamp: " << timestampUs
        << ", parameters: " << parametersToString(params);

    if (actionId == kIntegrationActionId)
    {
        std::string message =
            "Message generated by the Integration:\n" +
            indent(1) + "Action id: " + actionId + ",\n" +
            indent(1) + "Timestamp: " + nx::kit::utils::format("%lld us", timestampUs) + ",\n" +
            indent(1) + "Parameters: " + parametersToString(params) + ",\n" +
            indent(1) + "State: " + state;

        IAction::Result result;
        result.messageToUser = makePtr<String>(message);

        NX_PRINT << "Executing an Integration Action returning a message: "
            << nx::kit::utils::toString(message);

        return result;
    }


    return Result<IAction::Result>();
}

} // namespace integration_actions
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
