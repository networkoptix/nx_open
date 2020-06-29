// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

#include <nx/sdk/i_device_info.h>
#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/error.h>

#include "utils.h"
#include "device_agent.h"
#include "settings_model.h"
#include "stub_analytics_plugin_ini.h"
#include "objects/vehicles.h"
#include "objects/human_face.h"
#include "objects/stone.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

using namespace std::chrono;
using namespace std::literals::chrono_literals;

Engine::Engine(Plugin* plugin):
    nx::sdk::analytics::Engine(NX_DEBUG_ENABLE_OUTPUT),
    m_plugin(plugin)
{
    obtainPluginHomeDir();
    initCapabilities();

    m_pluginDiagnosticEventThread =
        std::make_unique<std::thread>([this]() { generatePluginDiagnosticEvents(); });
}

Engine::~Engine()
{
    {
        std::unique_lock<std::mutex> lock(m_pluginDiagnosticEventGenerationLoopMutex);
        m_terminated = true;
        m_pluginDiagnosticEventGenerationLoopCondition.notify_all();
    }
    if (m_pluginDiagnosticEventThread)
        m_pluginDiagnosticEventThread->join();
}

std::string Engine::capabilities() const
{
    std::string result;

    if (m_disableStreamSelection)
        result += "|disableStreamSelection";

    if (m_usePluginAsSettingsOriginForDeviceAgents)
        result += "|usePluginAsSettingsOrigin";

    if (result.size() > 1)
        result = result.substr(1, result.size() - 1);

    return result;
}

void Engine::generatePluginDiagnosticEvents()
{
    while (!m_terminated)
    {
        if (m_needToThrowPluginDiagnosticEvents)
        {
            pushPluginDiagnosticEvent(
                IPluginDiagnosticEvent::Level::info,
                "Info message from Engine",
                "Info message description");

            pushPluginDiagnosticEvent(
                IPluginDiagnosticEvent::Level::warning,
                "Warning message from Engine",
                "Warning message description");

            pushPluginDiagnosticEvent(
                IPluginDiagnosticEvent::Level::error,
                "Error message from Engine",
                "Error message description");
        }
        // Sleep until the next event pack needs to be generated, or the thread is ordered to
        // terminate (hence condition variable instead of sleep()). Return value (whether the
        // timeout has occurred) and spurious wake-ups are ignored.
        {
            std::unique_lock<std::mutex> lock(m_pluginDiagnosticEventGenerationLoopMutex);
            if (m_terminated)
                break;

            static const seconds kEventGenerationPeriod{7};
            m_pluginDiagnosticEventGenerationLoopCondition.wait_for(lock, kEventGenerationPeriod);
        }
    }
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    *outResult = new DeviceAgent(this, deviceInfo);
}

void Engine::obtainPluginHomeDir()
{
    const auto utilityProvider = m_plugin->utilityProvider();
    NX_KIT_ASSERT(utilityProvider);

    m_pluginHomeDir = utilityProvider->homeDir();

    if (m_pluginHomeDir.empty())
        NX_PRINT << "Plugin home dir: absent";
    else
        NX_PRINT << "Plugin home dir: " << nx::kit::utils::toString(m_pluginHomeDir);
}

void Engine::initCapabilities()
{
    if (ini().deviceDependent)
        m_capabilities += "|deviceDependent";

    if (ini().keepObjectBoundingBoxRotation)
        m_capabilities += "|keepObjectBoundingBoxRotation";

    if (ini().usePluginAsSettingsOrigin)
        m_capabilities += "|usePluginAsSettingsOrigin";

    m_streamTypeFilter = "motion|compressedVideo";

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
            m_streamTypeFilter = "motion|uncompressedVideo";
        }
    }

    if (ini().needMetadata)
        m_streamTypeFilter += "|metadata";

    // Delete first '|', if any.
    if (!m_capabilities.empty() && m_capabilities.at(0) == '|')
        m_capabilities.erase(0, 1);
}

std::string Engine::manifestString() const
{
    return /*suppress newline*/ 1 + (const char*) R"json(
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
        },
        {
            "id": ")json" + kSuspiciousNoiseEventType + R"json(",
            "name": "Suspicious noise",
            "groupId": ")json" + kSoundRelatedEventGroup + R"json("
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
        },
        {
            "id": ")json" + kMotionVisualizationObjectType + R"json(",
            "name": "Motion visualization object"
        },
        {
            "id": ")json" + kStoneObjectType + R"json(",
            "name": "Stone"
        },
        {
            "id": ")json" + kFixedObjectType + R"json(",
            "name": "Fixed object"
        }
    ],
    "groups": [
        {
            "id": ")json" + kSoundRelatedEventGroup + R"json(",
            "name": "Sound related events"
        }
    ],
    "capabilities": ")json" + m_capabilities + R"json(",
    "streamTypeFilter": ")json" + m_streamTypeFilter + R"json(",
    "preferredStream": ")json" + ini().preferredStream + R"json(",
    "objectActions": [
        {
            "id": "nx.stub.addToList",
            "name": "Add to list",
            "supportedObjectTypeIds": [
                ")json" + kCarObjectType + R"json("
            ],
            "requirements": {
                "capabilities": "needBestShotVideoFrame|needBestShotObjectMetadata|needFullTrack",
                "bestShotVideoFramePixelFormat": "yuv420"
            },
            "parametersModel": {
                "type": "Settings",
                "items": [
                    {
                        "type": "TextField",
                        "name": "testTextField",
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
                                "name": "testSpinBox",
                                "defaultValue": 42,
                                "minValue": 0,
                                "maxValue": 100
                            },
                            {
                                "type": "DoubleSpinBox",
                                "caption": "DoubleSpinBox Parameter",
                                "name": "testDoubleSpinBox",
                                "defaultValue": 3.1415,
                                "minValue": 0.0,
                                "maxValue": 100.0
                            },
                            {
                                "type": "ComboBox",
                                "name": "testComboBox",
                                "caption": "ComboBox Parameter",
                                "defaultValue": "value2",
                                "range": ["value1", "value2", "value3"]
                            },
                            {
                                "type": "CheckBox",
                                "caption": "CheckBox Parameter",
                                "name": "testCheckBox",
                                "defaultValue": true
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
    "deviceAgentSettingsModel": )json"
        + kRegularSettingsModelPart1
        + kEnglishCitiesPart
        + kRegularSettingsModelPart2 + R"json(
}
)json";
}

Result<const ISettingsResponse*> Engine::settingsReceived()
{
    m_disableStreamSelection = toBool(settingValue(kDisableStreamSelectionSetting));

    m_usePluginAsSettingsOriginForDeviceAgents =
        toBool(settingValue(kUsePluginAsSettingsOriginForDeviceAgents));

    m_needToThrowPluginDiagnosticEvents = toBool(
        settingValue(kThrowPluginDiagnosticEventsFromEngineSetting));

    if (m_needToThrowPluginDiagnosticEvents && !m_pluginDiagnosticEventThread)
    {
        NX_PRINT << __func__ << "(): Starting Plugin Diagnostic Event generation";
        m_needToThrowPluginDiagnosticEvents = true;
    }
    else if (!m_needToThrowPluginDiagnosticEvents && m_pluginDiagnosticEventThread)
    {
        NX_PRINT << __func__ << "(): Stopping Plugin Diagnostic Event generation";
        m_needToThrowPluginDiagnosticEvents = false;
        m_pluginDiagnosticEventGenerationLoopCondition.notify_all();
    }

    return nullptr;
}

static std::string timestampedObjectMetadataToString(
    Ptr<const ITimestampedObjectMetadata> metadata)
{
    if (!metadata)
        return "null";

    std::string attributeString;
    if (metadata->attributeCount() > 0)
    {
        attributeString += "\n    Attributes:\n";
        for (int i = 0; i < metadata->attributeCount(); ++i)
        {
            const Ptr<const IAttribute> attribute = metadata->attribute(i);
            if (!attribute)
            {
                attributeString += "null";
            }
            else
            {
                attributeString += "        "
                    + nx::kit::utils::toString(attribute->name())
                    + ": "
                    + nx::kit::utils::toString(attribute->value());
            }

            if (i < metadata->attributeCount() - 1)
                attributeString += "\n";
        }
    }

    return nx::kit::utils::format("timestamp: %lld, id: %s",
        metadata->timestampUs(), UuidHelper::toStdString(metadata->trackId()).c_str())
        + attributeString;
}

static std::string uncompressedVideoFrameToString(Ptr<const IUncompressedVideoFrame> frame)
{
    if (!frame)
        return "null";

    return nx::kit::utils::format("%dx%d %s",
        frame->width(), frame->height(), pixelFormatToStdString(frame->pixelFormat()).c_str());
}

Result<IAction::Result> Engine::executeAction(
    const std::string& actionId,
    Uuid trackId,
    Uuid /*deviceId*/,
    int64_t /*timestampUs*/,
    nx::sdk::Ptr<IObjectTrackInfo> objectTrackInfo,
    const std::map<std::string, std::string>& params)
{
    std::string messageToUser;
    std::string actionUrl;

    if (actionId == "nx.stub.addToList")
    {
        messageToUser = std::string("Track id: ") + UuidHelper::toStdString(trackId) + "\n\n";

        if (!objectTrackInfo)
        {
            messageToUser += "No object track info provided.\n\n";
        }
        else
        {
            messageToUser += std::string("Object track info:\n")
                + "    Best shot frame: "
                    + uncompressedVideoFrameToString(objectTrackInfo->bestShotVideoFrame()) + "\n"
                + "    Best shot metadata: "
                    + timestampedObjectMetadataToString(objectTrackInfo->bestShotObjectMetadata())
                + "\n\n";
        }

        if (!params.empty())
        {
            messageToUser += std::string("Your param values are:\n");
            bool first = true;
            for (const auto& entry: params)
            {
                const auto& parameterName = entry.first;
                const auto& parameterValue = entry.second;

                if (!first)
                    messageToUser += ",\n";
                else
                    first = false;
                messageToUser += parameterName + ": [" + parameterValue + "]";
            }
        }
        else
        {
            messageToUser += "No param values provided.";
        }

        NX_PRINT << __func__ << "(): Returning a message: "
            << nx::kit::utils::toString(messageToUser);
    }
    else if (actionId == "nx.stub.addPerson")
    {
        actionUrl = ini().addPersonActionUrlPrefix + UuidHelper::toStdString(trackId);
        NX_PRINT << __func__ << "(): Returning URL: " << nx::kit::utils::toString(actionUrl);
    }
    else
    {
        NX_PRINT << __func__ << "(): ERROR: Unsupported actionId.";
        return error(ErrorCode::invalidParams, "Unsupported actionId");
    }

    return IAction::Result{makePtr<String>(actionUrl), makePtr<String>(messageToUser)};
}

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
