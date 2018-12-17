#include "engine.h"

#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

#include <nx/sdk/i_device_info.h>
#include <nx/sdk/analytics/common/plugin.h>

#include "device_agent.h"
#include "stub_analytics_plugin_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine(IPlugin* plugin): nx::sdk::analytics::common::Engine(plugin, NX_DEBUG_ENABLE_OUTPUT)
{
    initCapabilities();
}

Engine::~Engine()
{
    m_terminated.store(true);
    m_pluginEventGenerationLoopCondition.notify_all();
    if (m_thread)
        m_thread->join();
}

nx::sdk::analytics::IDeviceAgent* Engine::obtainDeviceAgent(
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
        if (!nx::sdk::analytics::common::pixelFormatFromStdString(
            pixelFormatString, &m_pixelFormat))
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

void Engine::processPluginEvents()
{
    while (!m_terminated)
    {
        using namespace std::chrono_literals;

        pushPluginEvent(
            IPluginEvent::Level::info,
            "Info message from Engine",
            "Info message description");

        pushPluginEvent(
            IPluginEvent::Level::warning,
            "Warning message from Engine",
            "Warning message description");

        pushPluginEvent(
            IPluginEvent::Level::error,
            "Error message from Engine",
            "Error message description");

        // Sleep until the next event pack needs to be generated, or the thread is ordered to
        // terminate (hence condition variable instead of sleep()). Return value (whether the
        // timeout has occurred) and spurious wake-ups are ignored.
        static const std::chrono::seconds kEventGenerationPeriod{10};
        std::unique_lock<std::mutex> lock(m_pluginEventGenerationLoopMutex);
        m_pluginEventGenerationLoopCondition.wait_for(lock, kEventGenerationPeriod);
    }
}

std::string Engine::manifest() const
{
    return /*suppress newline*/1 + R"json(
{
    "eventTypes": [
        {
            "id": ")json" + kLineCrossingEventType + R"json(",
            "name": "Line crossing"
        },
        {
            "id": ")json" + kObjectInTheAreaEventType + R"json(",
            "name": "Object in the area",
            "flags": "stateDependent|regionDependent"
        }
    ],
    "objectTypes": [
        {
            "id": ")json" + kCarObjectType + R"json(",
            "name": "Car"
        },
        {
            "id": ")json" + kHumanFaceObjectType + R"json(",
            "name": "Human face"
        }
    ],
    "capabilities": ")json" + m_capabilities + R"json(",
    "objectActions": [
        {
            "id": "nx.stub.addToList",
            "name": "Add to list",
            "supportedObjectTypeIds": [
                ")json" + kCarObjectType + R"json("
            ],
            "parametersModel": {
                "type": "Settings",
                "items": [
                    {
                        "type": "TextField",
                        "name": "test_text_field",
                        "caption": "Text Field Parameter",
                        "description": "A text field",
                        "defaultValue": "a text"
                    },
                    {
                        "type": "GroupBox",
                        "caption": "Parameter Group",
                        "items": [
                            {
                                "type": "SpinBox",
                                "caption": "SpinBox Parameter",
                                "name": "test_spin_box",
                                "defaultValue": 42,
                                "minValue": 0,
                                "maxValue": 100
                            },
                            {
                                "type": "DoubleSpinBox",
                                "caption": "DoubleSpinBox Parameter",
                                "name": "test_double_spin_box",
                                "defaultValue": 3.1415,
                                "minValue": 0.0,
                                "maxValue": 100.0
                            },
                            {
                                "type": "ComboBox",
                                "name": "test_combo_box",
                                "caption": "ComboBox Parameter",
                                "defaultValue": "value2",
                                "range": ["value1", "value2", "value3"]
                            },
                            {
                                "type": "Row",
                                "items": [
                                    {
                                        "type": "CheckBox",
                                        "caption": "CheckBox Parameter",
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
        },
        {
            "id": "nx.stub.addPerson",
            "name": "Add person (URL-based)",
            "supportedObjectTypeIds": [
                ")json" + kCarObjectType + R"json("
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

void Engine::settingsReceived()
{
    if (ini().throwPluginEventsFromEngine)
    {
        NX_PRINT << __func__ << "(): Starting plugin event generation thread";
        if (!m_thread)
            m_thread.reset(new std::thread([this]() { processPluginEvents(); }));
    }
    else
    {
        NX_PRINT << __func__ << "()";
    }
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
} // namespace vms_server_plugins
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
                "name": "text",
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
    return new nx::sdk::analytics::common::Plugin(
        kLibName,
        kPluginManifest,
        [](nx::sdk::analytics::IPlugin* plugin)
        {
            return new nx::vms_server_plugins::analytics::stub::Engine(plugin);
        });
}

} // extern "C"
