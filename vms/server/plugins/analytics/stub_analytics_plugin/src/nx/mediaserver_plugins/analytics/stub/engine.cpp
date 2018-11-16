#include "engine.h"

#define NX_PRINT_PREFIX (this->utils.printPrefix)
#include <nx/kit/debug.h>

#include <nx/sdk/analytics/common_plugin.h>

#include "device_agent.h"
#include "stub_analytics_plugin_ini.h"

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace stub {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine(Plugin* plugin): CommonEngine(plugin, NX_DEBUG_ENABLE_OUTPUT)
{
    initCapabilities();
}

nx::sdk::analytics::DeviceAgent* Engine::obtainDeviceAgent(
    const DeviceInfo* /*deviceInfo*/, Error* /*outError*/)
{
    return new DeviceAgent(this);
}

void Engine::initCapabilities()
{
    if (ini().deviceModelDependent)
        m_capabilities += "|deviceModelDependent";

    const std::string pixelFormatString = ini().needUncompressedVideoFrames;
    if (!pixelFormatString.empty())
    {
        if (!pixelFormatFromStdString(pixelFormatString, &m_pixelFormat))
        {
            NX_PRINT << "ERROR: Invalid value of needUncompressedVideoFrames in "
                << ini().iniFile() << ": [" << pixelFormatString << "].";
        }
        else
        {
            m_needUncompressedVideoFrames = true;
            m_capabilities += std::string("|needUncompressedVideoFrames_") + pixelFormatString;
        }
    }

    // Delete first '|', if any.
    if (!m_capabilities.empty() && m_capabilities.at(0) == '|')
        m_capabilities.erase(0, 1);
}

std::string Engine::manifest() const
{
    return /*suppress newline*/1 + R"json(
{
    "pluginId": "nx.stub",
    "pluginName": {
        "value": "Stub Analytics Plugin"
    },
    "eventTypes": [
        {
            "id": ")json" + kLineCrossingEventType + R"json(",
            "name": {
                "value": "Line crossing"
            }
        },
        {
            "id": ")json" + kObjectInTheAreaEventType + R"json(",
            "name": {
                "value": "Object in the area"
            },
            "flags": "stateDependent|regionDependent"
        }
    ],
    "objectTypes": [
        {
            "id": ")json" + kCarObjectType+ R"json(",
            "name": {
                "value": "Car"
            }
        },
        {
            "id": ")json" + kHumanFaceObjectType+ R"json(",
            "name": {
                "value": "Human face"
            }
        }
    ],
    "capabilities": ")json" + m_capabilities + R"json(",
    "objectActions": [
        {
            "id": "nx.stub.addToList",
            "name": {
                "value": "Add to list"
            },
            "supportedObjectTypeIds": [
                ")json" + kCarObjectType+ R"json("
            ],
            "settings": {
                "params": [
                    {
                        "id": "paramA",
                        "dataType": "Number",
                        "name": "Param A",
                        "description": "Number A"
                    },
                    {
                        "id": "paramB",
                        "dataType": "Enumeration",
                        "range": "b1,b3",
                        "name": "Param B",
                        "description": "Enumeration B"
                    }
                ]
            }
        },
        {
            "id": "nx.stub.addPerson",
            "name": {
                "value": "Add person (URL-based)"
            },
            "supportedObjectTypeIds": [
                ")json" + kCarObjectType+ R"json("
            ]
        }
    ],
    "deviceAgentSettingsModel": {
        "type": "Settings",
        "items": [
            {
                "type": "TextField",
                "name": "test_text_field",
                "caption": "Device Agent Text Field",
                "description": "A text field",
                "defaultValue": "a text"
            },
            {
                "type": "GroupBox",
                "caption": "Device Agent Group",
                "items": [
                    {
                        "type": "SpinBox",
                        "caption": "Device Agent SpinBox",
                        "name": "test_spin_box",
                        "defaultValue": 42,
                        "minValue": 0,
                        "maxValue": 100
                    },
                    {
                        "type": "DoubleSpinBox",
                        "caption": "Device Agent DoubleSpinBox",
                        "name": "test_double_spin_box",
                        "defaultValue": 3.1415,
                        "minValue": 0.0,
                        "maxValue": 100.0
                    },
                    {
                        "type": "ComboBox",
                        "name": "test_combo_box",
                        "caption": "Device Agent ComboBox",
                        "defaultValue": "value2",
                        "range": ["value1", "value2", "value3"]
                    },
                    {
                        "type": "Row",
                        "items": [
                            {
                                "type": "CheckBox",
                                "caption": "Device Agent CheckBox",
                                "name": "test_check_box",
                                "defaultValue": true,
                                "value": true
                            }
                        ]
                    }
                ]
            }
        ]
    }
}
)json";
}

void Engine::settingsChanged()
{
    NX_PRINT << __func__ << "()";
}

void Engine::executeAction(
    const std::string& actionId,
    nxpl::NX_GUID objectId,
    nxpl::NX_GUID /*deviceId*/,
    int64_t /*timestampUs*/,
    const std::map<std::string, std::string>& params,
    std::string* outActionUrl,
    std::string* outMessageToUser,
    Error* error)
{
    if (actionId == "nx.stub.addToList")
    {
        std::string valueA;
        auto paramAIt = params.find("paramA");
        if (paramAIt != params.cend())
            valueA = paramAIt->second;

        std::string valueB;
        auto paramBIt = params.find("paramB");
        if (paramBIt != params.cend())
            valueB = paramBIt->second;

        *outMessageToUser = std::string("Your param values are: ")
            + "paramA: [" + valueA + "], "
            + "paramB: [" + valueB + "]";

        NX_PRINT << __func__ << "(): Returning a message: [" << *outMessageToUser << "]";
    }
    else if (actionId == "nx.stub.addPerson")
    {
        *outActionUrl = "http://internal.server/addPerson?objectId=" + nxpt::toStdString(objectId);
        NX_PRINT << __func__ << "(): Returning URL: [" << *outActionUrl << "]";
    }
    else
    {
        NX_PRINT << __func__ << "(): ERROR: Unsupported actionId.";
        *error = Error::unknownError;
    }
}

} // namespace stub
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx

namespace {

static const std::string kLibName = "stub_analytics_plugin";
static const std::string kPluginManifest = R"json(
{
    "id": "nx.stub",
    "name": "Stub analytics plugin",
    "version": "1.0.0",
    "engineSettingsModel": {
        "type": "Settings",
        "items": [
            {
                "type": "TextField",
                "name": "nx.stub.engine.settings.text_0",
                "caption": "Text Field",
                "description": "A text field",
                "defaultValue": "a text"
            },
            {
                "type": "GroupBox",
                "caption": "Group",
                "items": [
                    {
                        "type": "SpinBox",
                        "name": "test_spin_box",
                        "defaultValue": 42,
                        "minValue": 0,
                        "maxValue": 100
                    },
                    {
                        "type": "DoubleSpinBox",
                        "name": "test_double_spin_box",
                        "defaultValue": 3.1415,
                        "minValue": 0.0,
                        "maxValue": 100.0
                    },
                    {
                        "type": "ComboBox",
                        "name": "test_double_combo_box",
                        "defaultValue": "value2",
                        "range": ["value1", "value2", "value3"]
                    },
                    {
                        "type": "Row",
                        "items": [
                            {
                                "type": "CheckBox",
                                "name": "test_check_box",
                                "defaultValue": true,
                                "value": true
                            }
                        ]
                    }
                ]
            }
        ]
    }
})json";

} // namespace

extern "C" {

NX_PLUGIN_API nxpl::PluginInterface* createNxAnalyticsPlugin()
{
    return new nx::sdk::analytics::CommonPlugin(
        kLibName,
        kPluginManifest,
        [](nx::sdk::analytics::Plugin* plugin)
        {
            return new nx::mediaserver_plugins::analytics::stub::Engine(plugin);
        });
}

} // extern "C"
